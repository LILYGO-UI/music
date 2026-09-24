#ifndef LILYGO_UI_MUSIC_AUDIO_BACKEND_HPP
#define LILYGO_UI_MUSIC_AUDIO_BACKEND_HPP

#include <memory>
#include <string>

struct AudioSnapshot {
    std::string path;
    bool loading            = false;
    bool playing            = false;
    bool loaded             = false;
    bool finished           = false;
    double position_seconds = 0;
    double duration_seconds = 0;
    std::string error;
};

// Calls enqueue work; snapshot() never waits for media I/O. Implementations own
// and stop their playback resources before destruction.
class AudioBackend {
public:
    virtual ~AudioBackend()                                = default;
    virtual void load(const std::string& path, int volume) = 0;
    virtual void set_paused(bool paused)                   = 0;
    virtual void seek(double seconds)                      = 0;
    virtual void set_volume(int volume)                    = 0;
    virtual void stop()                                    = 0;
    [[nodiscard]] virtual AudioSnapshot snapshot() const   = 0;
};

// null_audio is for integration tests only, and explicitly disables sound.
[[nodiscard]] std::unique_ptr<AudioBackend> make_mpv_audio_backend(const std::string& executable = "mpv",
                                                                   bool null_audio               = false);

#endif
