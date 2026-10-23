#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "prx/libSceAvPlayer/include/HttpStream.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <exception>
#include <limits>
#include <iomanip>
#include <locale>
#include <map>
#include <set>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

extern "C" {
#include <libavformat/avio.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

namespace AvPlayer {
namespace {

class HttpFailure : public std::runtime_error {
public:
    explicit HttpFailure(const char* message) : std::runtime_error(message) {}
};

std::string Lower(std::string text) {
    for (char& byte : text) if (byte >= 'A' && byte <= 'Z') byte += 'a' - 'A';
    return text;
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}

std::uint64_t Number(std::string_view text, int base = 10) {
    std::uint64_t number = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), number, base);
    if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size()) throw HttpFailure("invalid HTTP size");
    return number;
}

std::optional<std::chrono::sys_seconds> HttpDate(const std::string& text) {
    std::tm time{};
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    input >> std::get_time(&time, "%a, %d %b %Y %H:%M:%S GMT");
    if (input.fail() || input.peek() != std::char_traits<char>::eof()) return {};
    const std::chrono::year_month_day day{std::chrono::year{time.tm_year + 1900}, std::chrono::month{static_cast<unsigned>(time.tm_mon + 1)}, std::chrono::day{static_cast<unsigned>(time.tm_mday)}};
    if (!day.ok() || time.tm_hour > 23 || time.tm_min > 59 || time.tm_sec > 60) return {};
    return std::chrono::sys_days{day} + std::chrono::hours{time.tm_hour} + std::chrono::minutes{time.tm_min} + std::chrono::seconds{time.tm_sec};
}

struct Url {
    std::string authority;
    std::string host;
    std::string port;
    std::string target;
};

Url Parse(const std::string& text) {
    if (Lower(text.substr(0, 8)) == "https://") NotImplemented_nid_no_patch("AvPlayer HTTPS sources");
    if (Lower(text.substr(0, 7)) != "http://") throw HttpFailure("unsupported HTTP source scheme");
    for (const unsigned char byte : text) {
        if (byte >= 0x80) NotImplemented_nid_no_patch("AvPlayer non-ASCII HTTP URLs");
        if (byte <= 0x20 || byte == 0x7f) throw HttpFailure("invalid HTTP URL byte");
    }
    const auto end = text.find_first_of("/?#", 7);
    Url url;
    url.authority = text.substr(7, end == std::string::npos ? end : end - 7);
    if (url.authority.find('@') != std::string::npos) NotImplemented_nid_no_patch("AvPlayer HTTP URL user information");
    if (url.authority.empty()) throw HttpFailure("invalid HTTP authority");
    url.port = "80";
    std::size_t colon;
    if (url.authority.front() == '[') {
        const auto close = url.authority.find(']');
        if (close == std::string::npos) throw HttpFailure("invalid IPv6 HTTP authority");
        url.host = url.authority.substr(1, close - 1);
        colon = close + 1;
        if (colon < url.authority.size() && url.authority[colon] != ':') throw HttpFailure("invalid HTTP port");
    } else {
        colon = url.authority.find(':');
        url.host = url.authority.substr(0, colon);
    }
    if (colon != std::string::npos && colon < url.authority.size()) {
        url.port = url.authority.substr(colon + 1);
        const auto port = Number(url.port);
        if (port == 0 || port > 65535) throw HttpFailure("invalid HTTP port");
    }
    if (url.host.empty()) throw HttpFailure("empty HTTP host");
    url.target = end == std::string::npos ? "/" : text.substr(end);
    url.target.erase(url.target.find('#') == std::string::npos ? url.target.size() : url.target.find('#'));
    if (url.target.empty() || url.target.front() != '/') url.target.insert(0, "/");
    return url;
}

std::string Redirect(const Url& base, std::string location) {
    const auto fragment = location.find('#');
    if (fragment != std::string::npos) location.erase(fragment);
    if (location.find(':') < location.find_first_of("/?")) return location;
    if (location.starts_with("//")) return "http:" + location;
    std::string target;
    if (location.empty()) target = base.target;
    else if (location.front() == '/') target = location;
    else if (location.front() == '?') target = base.target.substr(0, base.target.find('?')) + location;
    else {
        target = base.target.substr(0, base.target.find('?'));
        target.erase(target.rfind('/') + 1);
        target += location;
    }
    const auto queryAt = target.find('?');
    const std::string query = queryAt == std::string::npos ? "" : target.substr(queryAt);
    std::string input = target.substr(0, queryAt);
    std::string output;
    while (!input.empty()) {
        if (input.starts_with("../")) input.erase(0, 3);
        else if (input.starts_with("./")) input.erase(0, 2);
        else if (input.starts_with("/./")) input.erase(0, 2);
        else if (input == "/.") input = "/";
        else if (input.starts_with("/../") || input == "/..") {
            input.erase(0, 3);
            if (input.empty()) input = "/";
            const auto last = output.rfind('/');
            output.erase(last == std::string::npos ? 0 : last);
        } else if (input == "." || input == "..") input.clear();
        else {
            const auto next = input.find('/', input.front() == '/' ? 1 : 0);
            output += input.substr(0, next);
            input.erase(0, next == std::string::npos ? input.size() : next);
        }
    }
    return "http://" + base.authority + output + query;
}

class Connection {
public:
    Connection(const Url& url, std::atomic_bool& stop) : stop(stop) {
        addrinfo hint{};
        hint.ai_socktype = SOCK_STREAM;
        addrinfo* raw = nullptr;
        if (getaddrinfo(url.host.c_str(), url.port.c_str(), &hint, &raw) != 0) throw HttpFailure("HTTP name resolution failed");
        const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(raw, freeaddrinfo);
        for (auto* address = raw; address != nullptr; address = address->ai_next) {
            socket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            if (socket == invalid) continue;
#ifdef _WIN32
            u_long enabled = 1;
            if (ioctlsocket(socket, FIONBIO, &enabled) != 0) { Close(); continue; }
#else
            if (fcntl(socket, F_SETFL, fcntl(socket, F_GETFL, 0) | O_NONBLOCK) < 0) { Close(); continue; }
#endif
#ifdef SO_NOSIGPIPE
            const int enabled = 1;
            setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
            if (::connect(socket, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0) return;
#ifdef _WIN32
            const int connectError = WSAGetLastError();
            const bool pending = connectError == WSAEWOULDBLOCK || connectError == WSAEINPROGRESS || connectError == WSAEINTR;
#else
            const bool pending = errno == EINPROGRESS || errno == EINTR;
#endif
            if (pending) {
                try {
                    Wait(true);
                    int error = 0;
#ifdef _WIN32
                    int length = sizeof(error);
                    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == 0 && error == 0) return;
#else
                    socklen_t length = sizeof(error);
                    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0) return;
#endif
                } catch (...) {
                    Close();
                    throw;
                }
            }
            Close();
        }
        throw HttpFailure("HTTP connection failed");
    }

    ~Connection() {
        Close();
    }

    void Send(const std::string& data) {
        std::size_t sent = 0;
        while (sent < data.size()) {
            Wait(true);
#ifdef MSG_NOSIGNAL
            constexpr int flags = MSG_NOSIGNAL;
#else
            constexpr int flags = 0;
#endif
            const auto length = static_cast<int>(std::min<std::size_t>(data.size() - sent, std::numeric_limits<int>::max()));
            const auto result = ::send(socket, data.data() + sent, length, flags);
            if (result < 0 && Retry()) continue;
            if (result <= 0) throw HttpFailure("HTTP request failed");
            sent += static_cast<std::size_t>(result);
        }
    }

    std::string Line() {
        for (;;) {
            const auto end = pending.find("\r\n");
            if (end != std::string::npos) {
                auto line = pending.substr(0, end);
                pending.erase(0, end + 2);
                return line;
            }
            if (!Receive()) throw HttpFailure("truncated HTTP headers");
        }
    }

    void Append(std::vector<std::uint8_t>& body, std::uint64_t size) {
        while (size != 0) {
            if (pending.empty() && !Receive()) throw HttpFailure("truncated HTTP body");
            const auto take = static_cast<std::size_t>(std::min<std::uint64_t>(size, pending.size()));
            body.insert(body.end(), pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(take));
            pending.erase(0, take);
            size -= take;
        }
    }

    void UntilClosed(std::vector<std::uint8_t>& body) {
        do {
            body.insert(body.end(), pending.begin(), pending.end());
            pending.clear();
        } while (Receive());
    }

private:
    bool Receive() {
        std::array<char, 8192> bytes{};
        for (;;) {
            Wait(false);
            const auto count = ::recv(socket, bytes.data(), static_cast<int>(bytes.size()), 0);
            if (count < 0 && Retry()) continue;
            if (count < 0) throw HttpFailure("HTTP read failed");
            pending.append(bytes.data(), static_cast<std::size_t>(count));
            return count != 0;
        }
    }

    static bool Retry() {
#ifdef _WIN32
        const int error = WSAGetLastError();
        return error == WSAEWOULDBLOCK || error == WSAEINTR;
#else
        return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
    }

    void Wait(bool write) {
        for (;;) {
            if (stop.load()) throw HttpFailure("HTTP source stopped");
#ifdef _WIN32
            WSAPOLLFD entry{socket, static_cast<short>(write ? POLLWRNORM : POLLRDNORM), 0};
            const int result = WSAPoll(&entry, 1, 100);
#else
            pollfd entry{socket, static_cast<short>(write ? POLLOUT : POLLIN), 0};
            const int result = poll(&entry, 1, 100);
#endif
            if (result > 0) return;
            if (result < 0 && !Retry()) throw HttpFailure("HTTP socket polling failed");
        }
    }

    void Close() {
        if (socket == invalid) return;
#ifdef _WIN32
        closesocket(socket);
#else
        ::close(socket);
#endif
        socket = invalid;
    }
#ifdef _WIN32
    struct Runtime {
        Runtime() {
            WSADATA data{};
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw HttpFailure("socket initialization failed");
        }
        ~Runtime() { WSACleanup(); }
    } runtime;
    static constexpr SOCKET invalid = INVALID_SOCKET;
    SOCKET socket = invalid;
#else
    static constexpr int invalid = -1;
    int socket = invalid;
#endif
    std::string pending;
    std::atomic_bool& stop;
};

}

struct HttpStream::State {
    AVIOContext* context = nullptr;
    std::string url;
    std::string validator;
    std::string etag;
    std::string modified;
    std::vector<std::uint8_t> cache;
    std::uint64_t cacheStart = 0;
    std::uint64_t position = 0;
    std::uint64_t size = 0;
    bool initialized = false;
    std::exception_ptr failure;
    std::atomic_bool stopped = false;

    ~State() {
        if (context) {
            av_freep(&context->buffer);
            avio_context_free(&context);
        }
    }

    void Fetch(std::uint64_t start, std::uint64_t length) {
        std::set<std::string> visited;
        for (;;) {
            if (!visited.insert(url).second) throw HttpFailure("HTTP redirect loop");
            const auto parsed = Parse(url);
            Connection connection(parsed, stopped);
            connection.Send("GET " + parsed.target + " HTTP/1.1\r\nHost: " + parsed.authority + "\r\nRange: bytes=" + std::to_string(start) + "-" + std::to_string(start + length - 1) + (validator.empty() ? "" : "\r\nIf-Range: " + validator) + "\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
            std::map<std::string, std::string> headers;
            int status;
            for (;;) {
                const auto line = connection.Line();
                if (line.size() < 12 || (!line.starts_with("HTTP/1.1 ") && !line.starts_with("HTTP/1.0 "))) throw HttpFailure("invalid HTTP status line");
                status = static_cast<int>(Number(std::string_view(line).substr(9, 3)));
                headers.clear();
                for (;;) {
                    const auto field = connection.Line();
                    if (field.empty()) break;
                    const auto colon = field.find(':');
                    if (colon == std::string::npos || colon == 0) throw HttpFailure("invalid HTTP header");
                    const auto name = Lower(field.substr(0, colon));
                    const std::string value(Trim(std::string_view(field).substr(colon + 1)));
                    for (const unsigned char byte : value) if ((byte < 0x20 && byte != '\t') || byte == 0x7f) throw HttpFailure("invalid HTTP header value");
                    if (name != "content-range" && name != "content-length" && name != "content-encoding" && name != "transfer-encoding" && name != "location" && name != "etag" && name != "last-modified" && name != "date") continue;
                    const auto [found, inserted] = headers.emplace(name, value);
                    if (!inserted && found->second != value) throw HttpFailure("conflicting HTTP headers");
                }
                if (status < 100 || status >= 200) break;
                if (status == 101) throw HttpFailure("HTTP upgrade is not a media file");
            }
            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
                if (!headers.contains("location")) throw HttpFailure("HTTP redirect has no location");
                url = Redirect(parsed, headers.at("location"));
                continue;
            }
            if (status != 200 && status != 206) throw HttpFailure("HTTP source request was refused");
            if (headers.contains("content-encoding") && Lower(headers.at("content-encoding")) != "identity") NotImplemented_nid_no_patch("AvPlayer encoded HTTP representations");
            std::vector<std::uint8_t> body;
            if (headers.contains("transfer-encoding")) {
                if (Lower(headers.at("transfer-encoding")) != "chunked") NotImplemented_nid_no_patch("AvPlayer HTTP transfer coding");
                for (;;) {
                    const auto chunk = connection.Line();
                    const auto count = Number(Trim(std::string_view(chunk).substr(0, chunk.find(';'))), 16);
                    if (count == 0) {
                        while (!connection.Line().empty()) {}
                        break;
                    }
                    connection.Append(body, count);
                    if (!connection.Line().empty()) throw HttpFailure("invalid HTTP chunk terminator");
                }
            } else if (headers.contains("content-length")) connection.Append(body, Number(headers.at("content-length")));
            else connection.UntilClosed(body);
            std::uint64_t total = body.size();
            std::uint64_t first = 0;
            if (status == 206) {
                if (!headers.contains("content-range")) throw HttpFailure("HTTP range response has no bounds");
                const auto& range = headers.at("content-range");
                const auto dash = range.find('-');
                const auto slash = range.find('/');
                if (!range.starts_with("bytes ") || dash == std::string::npos || slash <= dash) throw HttpFailure("invalid HTTP content range");
                first = Number(std::string_view(range).substr(6, dash - 6));
                const auto last = Number(std::string_view(range).substr(dash + 1, slash - dash - 1));
                total = Number(std::string_view(range).substr(slash + 1));
                if (first != start || last < first || last >= total || last - first + 1 != body.size() || body.size() > length) throw HttpFailure("HTTP range response does not match request");
            }
            if (total > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) || (initialized && total != size)) throw HttpFailure("HTTP source size changed or is not representable");
            if (initialized && ((!etag.empty() && headers.contains("etag") && headers.at("etag") != etag) || (!modified.empty() && headers.contains("last-modified") && headers.at("last-modified") != modified))) throw HttpFailure("HTTP source representation changed");
            if (!initialized) {
                if (headers.contains("etag")) etag = headers.at("etag");
                if (headers.contains("last-modified")) modified = headers.at("last-modified");
                if (etag.starts_with('"') && etag.ends_with('"')) validator = etag;
                else if (etag.empty() && headers.contains("date")) {
                    const auto date = HttpDate(headers.at("date"));
                    const auto last = HttpDate(modified);
                    if (date && last && *date - *last >= std::chrono::seconds{60}) validator = modified;
                }
            }
            cache = std::move(body);
            cacheStart = first;
            size = total;
            initialized = true;
            return;
        }
    }

    static int Read(void* opaque, std::uint8_t* destination, int capacity) {
        auto& state = *static_cast<State*>(opaque);
        try {
            if (state.position >= state.size) return AVERROR_EOF;
            if (state.position < state.cacheStart || state.position - state.cacheStart >= state.cache.size()) state.Fetch(state.position, std::min<std::uint64_t>(65536, state.size - state.position));
            const auto offset = static_cast<std::size_t>(state.position - state.cacheStart);
            const auto count = std::min<std::size_t>(static_cast<std::size_t>(capacity), state.cache.size() - offset);
            if (count == 0) return AVERROR(EIO);
            std::memcpy(destination, state.cache.data() + offset, count);
            state.position += count;
            return static_cast<int>(count);
        } catch (...) {
            state.failure = std::current_exception();
            return AVERROR(EIO);
        }
    }

    static std::int64_t Seek(void* opaque, std::int64_t offset, int whence) {
        auto& state = *static_cast<State*>(opaque);
        if (whence == AVSEEK_SIZE) return static_cast<std::int64_t>(state.size);
        whence &= ~AVSEEK_FORCE;
        std::int64_t base;
        if (whence == SEEK_SET) base = 0;
        else if (whence == SEEK_CUR) base = static_cast<std::int64_t>(state.position);
        else if (whence == SEEK_END) base = static_cast<std::int64_t>(state.size);
        else return AVERROR(EINVAL);
        if (offset < -base || offset > std::numeric_limits<std::int64_t>::max() - base) return AVERROR(EINVAL);
        state.position = static_cast<std::uint64_t>(base + offset);
        return static_cast<std::int64_t>(state.position);
    }
};

HttpStream::HttpStream() : state(std::make_unique<State>()) {}
HttpStream::~HttpStream() = default;

bool HttpStream::Open(const std::string& url) {
    state->url = url;
    try {
        state->Fetch(0, 65536);
    } catch (const HttpFailure&) {
        return false;
    }
    auto* buffer = static_cast<std::uint8_t*>(av_malloc(65536));
    if (!buffer) return false;
    state->context = avio_alloc_context(buffer, 65536, 0, state.get(), State::Read, nullptr, State::Seek);
    if (!state->context) av_free(buffer);
    return state->context != nullptr;
}

AVIOContext* HttpStream::Context() const { return state->context; }
void HttpStream::Rethrow() const { if (state->failure) std::rethrow_exception(state->failure); }
void HttpStream::Start() {
    state->failure = nullptr;
    state->stopped.store(false);
    if (state->context) {
        state->context->error = 0;
        state->context->eof_reached = 0;
    }
}
void HttpStream::Stop() { state->stopped.store(true); }

}
