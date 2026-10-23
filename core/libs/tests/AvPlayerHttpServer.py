import http.server
import pathlib
import re
import subprocess
import sys
import threading
import time
import urllib.parse


requests = []


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        path = urllib.parse.urlsplit(self.path).path
        requests.append((path, self.headers.get("Range")))
        if path.startswith("/redirect/"):
            self.send_response(307)
            self.send_header("Location", "../range/avplayer.mp4?redirected=1")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        content = pathlib.Path("app0", pathlib.PurePosixPath(path).name).read_bytes()
        match = re.fullmatch(r"bytes=(\d+)-(\d+)", self.headers.get("Range", ""))
        assert match is not None
        start, end = map(int, match.groups())
        if path.startswith("/stall/") and start:
            time.sleep(2)
        if path.startswith("/ignore/"):
            start, end = 0, len(content) - 1
            self.send_response(200)
        else:
            self.send_response(206)
            end = min(end, start + 511, len(content) - 1)
            first = start + 1 if path.startswith("/bad/") or path.startswith("/bad-late/") and start else start
            self.send_header("Content-Range", f"bytes {first}-{end}/{len(content)}")
        if start:
            expected = "Wed, 01 Jan 2020 00:00:00 GMT" if path.startswith("/modified/") else '"fixture-v1"'
            assert self.headers.get("If-Range") == expected
        if path.startswith("/modified/"):
            self.send_header("Last-Modified", "Wed, 01 Jan 2020 00:00:00 GMT")
        else:
            self.send_header("ETag", '"fixture-v2"' if path.startswith("/changed/") and start else '"fixture-v1"')
        body = content[start:end + 1]
        if path.startswith("/chunk/"):
            self.send_header("Transfer-Encoding", "chunked")
        else:
            self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        if path.startswith("/chunk/"):
            for at in range(0, len(body), 19):
                chunk = body[at:at + 19]
                self.wfile.write(f"{len(chunk):X} ;test=1\r\n".encode() + chunk + b"\r\n")
            self.wfile.write(b"0\r\nX-Trailer: yes\r\n\r\n")
        else:
            self.wfile.write(body)


with http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    arguments = [sys.argv[1], f"http://127.0.0.1:{server.server_port}"] if len(sys.argv) > 2 else [sys.argv[1], "--http", f"http://127.0.0.1:{server.server_port}"]
    result = subprocess.run(arguments)
    server.shutdown()
    thread.join()
    if result.returncode:
        sys.exit(result.returncode)
    assert any(header is not None and not header.startswith("bytes=0-") for _, header in requests)
    if len(sys.argv) == 2:
        assert {"/range/avplayer.mp4", "/chunk/avplayer.webm", "/ignore/avplayer.mp4", "/redirect/avplayer.mp4"}.issubset({path for path, _ in requests})
