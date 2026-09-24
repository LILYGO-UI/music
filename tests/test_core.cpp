#include "pages/music/music_model.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

class FakeBackend final : public AudioBackend {
public:
    AudioSnapshot state;
    int loads        = 0;
    int last_volume  = 0;
    double last_seek = -1;
    void load(const std::string& path, int volume) override
    {
        ++loads;
        last_volume  = volume;
        state        = {};
        state.path   = path;
        state.loaded = state.playing = true;
        state.duration_seconds       = 120;
    }
    void set_paused(bool paused) override
    {
        state.playing = !paused;
    }
    void seek(double seconds) override
    {
        last_seek = state.position_seconds = seconds;
    }
    void set_volume(int volume) override
    {
        last_volume = volume;
    }
    void stop() override
    {
        state = {};
    }
    AudioSnapshot snapshot() const override
    {
        return state;
    }
    void finish()
    {
        state.finished = true;
        state.playing = state.loaded = false;
    }
};

std::vector<MusicTrack> fixture_tracks()
{
    return {{"one", "/music/one.wav", "One", "Artist", "Album", 120},
            {"two", "/music/two.wav", "Two", "Artist", "Album", 120},
            {"three", "/music/three.wav", "Three", "Artist", "Album", 120}};
}

void test_commands()
{
    auto backend = std::make_unique<FakeBackend>();
    auto* fake   = backend.get();
    MusicModel model(std::move(backend), fixture_tracks());
    assert(model.current_index() == 0 && !model.playing());
    model.toggle_playback();
    assert(model.playing() && fake->loads == 1);
    model.toggle_playback();
    model.poll();
    assert(!model.playing());
    model.toggle_playback();
    model.poll();
    assert(model.playing() && fake->loads == 1);
    model.seek(37);
    model.poll();
    assert(model.position_seconds() == 37);
    model.previous();
    assert(fake->last_seek == 0 && model.current_index() == 0);
    model.previous();
    assert(model.current_index() == 2);
    model.next();
    assert(model.current_index() == 0);
    model.seek(10000);
    assert(fake->last_seek <= 120);
    model.seek(-100);
    assert(fake->last_seek == 0);
    model.seek(std::numeric_limits<double>::quiet_NaN());
    assert(fake->last_seek == 0);
    model.set_volume(120);
    assert(model.volume() == 100 && fake->last_volume == 100);
    model.set_volume(-1);
    assert(model.volume() == 0);
    model.toggle_favorite();
    assert(model.current_track()->favorite);
    model.next();
    assert(!model.current_track()->favorite);
    model.play_track(0);
    assert(model.current_track()->favorite);
    model.play_track(999);
    assert(model.current_index() == 0);
    fake->state.error   = "device unavailable";
    fake->state.playing = false;
    model.poll();
    assert(model.error() == "device unavailable" && !model.playing());
}

void test_completion_and_shuffle()
{
    auto backend = std::make_unique<FakeBackend>();
    auto* fake   = backend.get();
    MusicModel model(std::move(backend), fixture_tracks());
    model.play_track(2);
    fake->finish();
    model.poll();
    assert(model.current_index() == 0 && model.playing());
    model.cycle_repeat();
    assert(model.repeat_mode() == RepeatMode::one);
    fake->finish();
    model.poll();
    assert(model.current_index() == 0 && fake->loads == 3);
    model.next();  // Explicit skip works even when repeat-one is enabled.
    assert(model.current_index() == 1);
    model.cycle_repeat();
    assert(model.repeat_mode() == RepeatMode::off);
    model.play_track(2);
    fake->finish();
    model.poll();
    assert(!model.playing() && model.current_index() == 2);
    const auto loads = fake->loads;
    model.poll();
    assert(fake->loads == loads);
    model.play_track(0);
    model.toggle_shuffle();
    std::set<std::size_t> visited{model.current_index()};
    for (int i = 0; i < 2; ++i) {
        fake->finish();
        model.poll();
        visited.insert(model.current_index());
    }
    assert(visited.size() == 3);
    const auto last = model.current_index();
    fake->finish();
    model.poll();
    assert(!model.playing() && model.current_index() == last);
    model.previous();
    assert(model.current_index() != last && model.playing());
}

void test_empty_library()
{
    MusicModel model(std::make_unique<FakeBackend>());
    model.toggle_playback();
    model.next();
    model.previous();
    model.toggle_favorite();
    model.seek(15);
    model.poll();
    assert(!model.current_track() && !model.playing() && !model.scanning());
    assert(model.library_label() == "还没有音乐");
}

struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory()
    {
        char name[]        = "/tmp/lilygo-music-tests-XXXXXX";
        const auto* result = ::mkdtemp(name);
        assert(result);
        path = result;
    }
    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

void write_wav(const std::filesystem::path& path)
{
    std::ofstream output(path, std::ios::binary);
    auto u16 = [&](unsigned value) {
        output.put(value & 255);
        output.put((value >> 8) & 255);
    };
    auto u32 = [&](unsigned value) {
        u16(value & 65535);
        u16(value >> 16);
    };
    constexpr unsigned rate = 8000, bytes = rate * 2 * 2;
    output.write("RIFF", 4);
    u32(36 + bytes);
    output.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(1);
    u32(rate);
    u32(rate * 2);
    u16(2);
    u16(16);
    output.write("data", 4);
    u32(bytes);
    for (unsigned i = 0; i < bytes; ++i) output.put(0);
}

void test_directory_scanner()
{
    TemporaryDirectory directory;
    std::filesystem::create_directory(directory.path / "Album");
    std::filesystem::create_directory(directory.path / ".hidden");
    write_wav(directory.path / "Album" / "Artist - A song.WAV");
    std::ofstream(directory.path / "Z song.FLAC").put('x');
    std::ofstream(directory.path / "sample.AC3").put('x');
    std::ofstream(directory.path / "sample.MP4").put('x');
    std::ofstream(directory.path / "sample.TS").put('x');
    std::ofstream(directory.path / "notes.txt").put('x');
    std::ofstream(directory.path / ".hidden" / "Invisible.mp3").put('x');
    std::filesystem::create_directory_symlink(directory.path, directory.path / "loop");
    const auto tracks = MusicModel::scan_directory(directory.path.string());
    assert(tracks.size() == 5);
    assert(tracks[0].title == "A song" && tracks[0].artist == "Artist" && tracks[0].album == "Album");
    assert(std::abs(tracks[0].duration_seconds - 2) < 0.001);
    assert(tracks[1].title == "sample" && tracks[2].title == "sample" && tracks[3].title == "sample");
    assert(tracks[4].title == "Z song" && !tracks[4].sample);
    assert(MusicModel::scan_directory((directory.path / "missing").string()).empty());
}

bool wait_for(const std::function<bool()>& condition, int timeout_ms = 5000)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    do {
        if (condition()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    } while (std::chrono::steady_clock::now() < deadline);
    return condition();
}

struct EnvironmentOverride {
    std::string name;
    std::string previous;
    bool existed;
    EnvironmentOverride(std::string key, const std::string& value) : name(std::move(key))
    {
        const auto* original = std::getenv(name.c_str());
        existed              = original != nullptr;
        if (original) previous = original;
        ::setenv(name.c_str(), value.c_str(), 1);
    }
    ~EnvironmentOverride()
    {
        if (existed)
            ::setenv(name.c_str(), previous.c_str(), 1);
        else
            ::unsetenv(name.c_str());
    }
};

void test_async_scan_and_preferences()
{
    TemporaryDirectory directory;
    const auto library = directory.path / "library";
    std::filesystem::create_directory(library);
    const auto track = library / "A quoted \"song\".wav";
    write_wav(track);
    EnvironmentOverride music_directory("LILYGO_MUSIC_DIR", library.string());
    EnvironmentOverride config_directory("XDG_CONFIG_HOME", (directory.path / "config").string());
    {
        MusicModel model;
        assert(wait_for([&] {
            model.poll();
            return !model.scanning();
        }));
        assert(model.tracks().size() == 1 && !model.current_track()->sample);
        model.toggle_favorite();
        model.set_volume(27);
        model.toggle_shuffle();
        model.cycle_repeat();
    }
    {
        MusicModel model;
        assert(wait_for([&] {
            model.poll();
            return !model.scanning();
        }));
        assert(model.current_track()->favorite && model.volume() == 27);
        assert(model.shuffle() && model.repeat_mode() == RepeatMode::one);
    }
    auto backend = std::make_unique<FakeBackend>();
    auto* fake   = backend.get();
    MusicModel model(std::move(backend), MusicModel::scan_directory(library.string()));
    model.toggle_playback();
    model.rescan();
    assert(wait_for([&] {
        model.poll();
        return !model.scanning();
    }));
    assert(model.playing() && fake->loads == 1);
    std::filesystem::remove(track);
    model.rescan();
    assert(wait_for([&] {
        model.poll();
        return !model.scanning();
    }));
    assert(model.tracks().empty() && !model.playing() && model.error().empty());
    // An explicitly empty user directory must never be replaced with samples.
    assert(model.library_label() == "还没有音乐");
}

std::string mpv_executable()
{
    const char* path = std::getenv("PATH");
    if (!path) return {};
    std::string entries(path);
    std::size_t begin = 0;
    while (begin <= entries.size()) {
        const auto end        = entries.find(':', begin);
        const auto directory  = entries.substr(begin, end == std::string::npos ? end : end - begin);
        const auto executable = directory + "/mpv";
        if (::access(executable.c_str(), X_OK) == 0) return executable;
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return {};
}

void test_backend_errors()
{
    auto backend = make_mpv_audio_backend("/does/not/exist/lilygo-mpv");
    backend->load("/music/sample.wav", 65);
    assert(wait_for([&] { return !backend->snapshot().error.empty(); }));
    assert(!backend->snapshot().playing && !backend->snapshot().loading);
}

void test_real_mpv_ipc()
{
    const auto executable = mpv_executable();
    if (executable.empty()) return;  // mpv is a runtime dependency, not a build dependency.
    TemporaryDirectory directory;
    // Quotes and shell metacharacters are ordinary filename bytes, never code.
    const auto path = directory.path / "a song ' $(touch nope).wav";
    write_wav(path);
    auto backend = make_mpv_audio_backend(executable, true);
    backend->load(path.string(), 45);
    assert(wait_for([&] {
        const auto state = backend->snapshot();
        return state.loaded && state.duration_seconds > 0;
    }));
    assert(std::abs(backend->snapshot().duration_seconds - 2) < 0.1);
    backend->set_paused(true);
    assert(wait_for([&] { return !backend->snapshot().playing; }));
    backend->seek(1.0);
    assert(wait_for([&] { return backend->snapshot().position_seconds > 0.9; }));
    assert(!backend->snapshot().playing);
    backend->set_paused(false);
    assert(wait_for([&] { return backend->snapshot().finished; }));
    assert(backend->snapshot().error.empty());
    assert(!std::filesystem::exists(directory.path / "nope"));
    backend->load((directory.path / "missing.mp3").string(), 50);
    assert(wait_for([&] { return !backend->snapshot().error.empty(); }));
    assert(!backend->snapshot().playing);
    backend->stop();
    assert(backend->snapshot().path.empty());
    backend->load((directory.path / "missing-again.mp3").string(), 50);
    backend->load(path.string(), 50);
    assert(wait_for([&] { return backend->snapshot().loaded; }));
    assert(backend->snapshot().path == path.string() && backend->snapshot().error.empty());
}

}  // namespace

int main()
{
    test_commands();
    test_completion_and_shuffle();
    test_empty_library();
    test_directory_scanner();
    test_async_scan_and_preferences();
    test_backend_errors();
    test_real_mpv_ipc();
    return 0;
}
