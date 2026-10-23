#include "Ngs2Test.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>

namespace {

std::vector<std::uint8_t> Encode(std::span<const float> samples) {
    std::vector<std::uint8_t> bytes(1 + samples.size() * 4, 0xa5);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const auto value = std::bit_cast<std::uint32_t>(samples[i]);
        for (unsigned byte = 0; byte < 4; ++byte) bytes[1 + i * 4 + byte] = static_cast<std::uint8_t>(value >> (byte * 8));
    }
    return bytes;
}

uintptr_t Sampler(uintptr_t system, std::uint32_t channels, std::uint32_t rate = 48000) {
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels, rate, 0, 0, 0}});
    Patch(voice, Mastering(system, channels));
    std::vector<float> identity(channels * channels, 0);
    for (std::uint32_t channel = 0; channel < channels; ++channel) identity[channel * channels + channel] = 1;
    Control(voice, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, channels * channels, identity.data()});
    Control(voice, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    return voice;
}

void Add(uintptr_t voice, const void* data, const Ngs2WaveformBlock& block, std::uint32_t flags = 0) {
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, data, flags, 1, &block});
}

Ngs2SamplerVoiceState State(uintptr_t voice) {
    Ngs2SamplerVoiceState result{};
    Require(sceNgs2VoiceGetState(voice, &result.voice_state, sizeof(result)) == SCE_NGS2_OK);
    return result;
}

std::vector<float> Render(uintptr_t system, std::uint32_t channels) {
    std::vector<float> output(Grain * channels + 2, 1234.0f);
    const Ngs2RenderBufferInfo info{output.data() + 1, Grain * channels * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    Require(output.front() == 1234.0f && output.back() == 1234.0f);
    return {output.begin() + 1, output.end() - 1};
}

std::vector<Ngs2VoiceCallbackInfo> callbacks;

void APS5_VABI Callback(const Ngs2VoiceCallbackInfo* info) {
    callbacks.push_back(*info);
}

void RegisterCallback(uintptr_t voice) {
    callbacks.clear();
    Control(voice, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, Callback, 0x55, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT | SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
}

void TestChannels() {
    for (std::uint32_t channels = 1; channels <= 8; ++channels) {
        const auto system = CreateSystem();
        const auto voice = Sampler(system, channels);
        std::vector<float> samples(Grain * channels);
        for (std::uint32_t i = 0; i < Grain; ++i) {
            for (std::uint32_t channel = 0; channel < channels; ++channel) samples[i * channels + channel] = (static_cast<int>((i * 3 + channel * 5) % 17) - 8) / 16.0f;
        }
        auto bytes = Encode(samples);
        const auto original = bytes;
        const Ngs2WaveformBlock block{1, samples.size() * 4, 0, 0, Grain, 0, 0x123};
        Add(voice, bytes.data(), block);
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        Require(Render(system, channels) == samples);
        const auto state = State(voice);
        Require(state.num_decoded_samples == Grain && state.decoded_data_size == samples.size() * 4);
        Require(state.waveform_data == bytes.data() + bytes.size() && Flags(voice) == 0 && bytes == original);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    }
}

void TestSkipRepeatAndCallbacks() {
    const auto system = CreateSystem();
    const auto voice = Sampler(system, 2);
    const std::array<float, 14> samples{5, 6, 7, 8, 0.125f, -0.25f, 0.25f, -0.5f, 0.375f, -0.75f, 0.5f, -1, 0.625f, -1.25f};
    auto bytes = Encode(samples);
    const Ngs2WaveformBlock block{1, sizeof(samples), 1, 2, 5, 0, 0xa5};
    Add(voice, bytes.data(), block);
    RegisterCallback(voice);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    const auto first = Render(system, 2);
    for (std::size_t frame = 0; frame < Grain; ++frame) {
        for (std::size_t channel = 0; channel < 2; ++channel) Require(first[frame * 2 + channel] == samples[(2 + frame % 5) * 2 + channel]);
    }
    auto state = State(voice);
    Require(state.num_decoded_samples == 8 && state.decoded_data_size == 64 && state.waveform_data == bytes.data() + 1 + 5 * 8);
    Require(callbacks.size() == 1 && callbacks[0].flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT && callbacks[0].num_repeated == 1);
    const auto second = Render(system, 2);
    for (std::size_t frame = 0; frame < Grain; ++frame) {
        for (std::size_t channel = 0; channel < 2; ++channel) Require(second[frame * 2 + channel] == (frame < 2 ? samples[(5 + frame) * 2 + channel] : 0.0f));
    }
    state = State(voice);
    Require(state.num_decoded_samples == 10 && state.decoded_data_size == 80 && state.waveform_data == bytes.data() + bytes.size());
    Require(callbacks.size() == 2 && callbacks[1].flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END && Flags(voice) == 0);
    for (const auto& callback : callbacks) Require(callback.callback_data == 0x55 && callback.user_data == 0xa5 && callback.block_data == bytes.data() + 1 && callback.block_size == sizeof(samples));
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

void TestResampling() {
    const auto system = CreateSystem();
    const auto voice = Sampler(system, 1, 24000);
    const std::array<float, 4> samples{0, 0.5f, 1, -0.5f};
    auto bytes = Encode(samples);
    Add(voice, bytes.data(), {1, sizeof(samples), 0, 0, 4, 0, 0});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    const std::vector<float> expected{0, 0.25f, 0.5f, 0.75f, 1, 0.25f, -0.5f, -0.5f};
    Require(Render(system, 1) == expected);
    const auto state = State(voice);
    Require(state.num_decoded_samples == 4 && state.decoded_data_size == 16 && state.waveform_data == bytes.data() + bytes.size());
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

void TestIntegerOutput() {
    const auto system = CreateSystem();
    const auto voice = Sampler(system, 1);
    const std::array<float, Grain> samples{-1.25f, -1, -0.5f, -0.125f, 0, 0.25f, 0.75f, 1.5f};
    auto bytes = Encode(samples);
    Add(voice, bytes.data(), {1, sizeof(samples), 0, 0, Grain, 0, 0});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    std::array<std::int16_t, Grain + 2> output;
    output.fill(1234);
    const Ngs2RenderBufferInfo info{output.data() + 1, Grain * sizeof(std::int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    const std::array<std::int16_t, Grain> expected{-32768, -32768, -16384, -4096, 0, 8192, 24576, 32767};
    Require(std::equal(expected.begin(), expected.end(), output.begin() + 1));
    Require(output.front() == 1234 && output.back() == 1234);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

void TestStreaming(bool closeEarly) {
    const auto system = CreateSystem();
    const auto voice = Sampler(system, 2);
    const std::array<float, 6> firstSamples{0.125f, -0.125f, 0.25f, -0.25f, 0.375f, -0.375f};
    const std::array<float, 8> secondSamples{0.5f, -0.5f, 0.625f, -0.625f, 0.75f, -0.75f, 0.875f, -0.875f};
    const std::array<float, 6> lastSamples{0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f};
    auto first = Encode(firstSamples), second = Encode(secondSamples), last = Encode(lastSamples);
    const auto duration = closeEarly ? 100u : 10u;
    Add(voice, first.data(), {1, sizeof(firstSamples), 0, 0, duration, 0, 0}, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    auto output = Render(system, 2);
    Require(std::equal(firstSamples.begin(), firstSamples.end(), output.begin()));
    Require(std::all_of(output.begin() + firstSamples.size(), output.end(), [](float f) { return f == 0; }));
    auto state = State(voice);
    Require(state.num_decoded_samples == 3 && state.decoded_data_size == 24 && state.waveform_data == first.data() + first.size());
    output = Render(system, 2);
    Require(std::all_of(output.begin(), output.end(), [](float f) { return f == 0; }));
    Require(State(voice).decoded_data_size == 24);
    bool rejected = false;
    try { Add(voice, second.data(), {1, 7, 0, 0, duration, 0, 0}, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected && State(voice).decoded_data_size == 24);
    Add(voice, second.data(), {1, sizeof(secondSamples), 0, 0, duration, 0, 0}, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND | (closeEarly ? 0 : SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE));
    Require(State(voice).waveform_data == second.data() + 1);
    output = Render(system, 2);
    Require(std::equal(secondSamples.begin(), secondSamples.end(), output.begin()));
    Require(std::all_of(output.begin() + secondSamples.size(), output.end(), [](float f) { return f == 0; }));
    state = State(voice);
    Require(state.num_decoded_samples == 7 && state.decoded_data_size == 56 && state.waveform_data == second.data() + second.size());
    if (!closeEarly) {
        Add(voice, last.data(), {1, sizeof(lastSamples), 0, 0, duration, 0, 0}, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
        output = Render(system, 2);
        Require(std::equal(lastSamples.begin(), lastSamples.end(), output.begin()));
        Require(std::all_of(output.begin() + lastSamples.size(), output.end(), [](float f) { return f == 0; }));
        state = State(voice);
        Require(state.num_decoded_samples == 10 && state.decoded_data_size == 80 && state.waveform_data == last.data() + last.size());
    }
    Require(Flags(voice) == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

void TestNonfinite() {
    for (const float sample : {std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        const auto system = CreateSystem();
        const auto voice = Sampler(system, 1);
        const std::array<float, Grain> samples{sample};
        auto bytes = Encode(samples);
        Add(voice, bytes.data(), {1, sizeof(samples), 0, 0, Grain, 0, 0});
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        std::array<float, Grain> output;
        output.fill(1234);
        const Ngs2RenderBufferInfo info{output.data(), sizeof(output), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
        bool rejected = false;
        try { sceNgs2SystemRender(system, &info, 1); } catch (const std::runtime_error&) { rejected = true; }
        Require(rejected && std::all_of(output.begin(), output.end(), [](float f) { return f == 1234; }));
        Require(State(voice).num_decoded_samples == 0 && State(voice).decoded_data_size == 0);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    }
}

} // namespace

int main() {
    TestChannels();
    TestSkipRepeatAndCallbacks();
    TestResampling();
    TestIntegerOutput();
    TestStreaming(false);
    TestStreaming(true);
    TestNonfinite();
}
