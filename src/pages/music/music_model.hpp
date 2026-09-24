#ifndef LILYGO_UI_MUSIC_MUSIC_MODEL_HPP
#define LILYGO_UI_MUSIC_MUSIC_MODEL_HPP

#include "pages/music/audio_backend.hpp"

#include <atomic>
#include <cstddef>
#include <future>
#include <memory>
#include <random>
#include <string>
#include <vector>

struct MusicTrack {
    std::string id;
    std::string path;
    std::string title;
    std::string artist;
    std::string album;
    double duration_seconds = 0;
    bool favorite           = false;
    bool sample             = false;
    unsigned artwork        = 0;
};

enum class RepeatMode { off, all, one };

class MusicModel {
public:
    MusicModel();
    explicit MusicModel(std::unique_ptr<AudioBackend> backend, std::vector<MusicTrack> initial_tracks = {});
    ~MusicModel();
    MusicModel(const MusicModel&)            = delete;
    MusicModel& operator=(const MusicModel&) = delete;

    [[nodiscard]] const std::vector<MusicTrack>& tracks() const noexcept;
    [[nodiscard]] const MusicTrack* current_track() const noexcept;
    [[nodiscard]] std::size_t current_index() const noexcept;
    [[nodiscard]] bool playing() const noexcept;
    [[nodiscard]] bool loading() const noexcept;
    [[nodiscard]] bool scanning() const noexcept;
    [[nodiscard]] bool shuffle() const noexcept;
    [[nodiscard]] RepeatMode repeat_mode() const noexcept;
    [[nodiscard]] int volume() const noexcept;
    [[nodiscard]] double position_seconds() const noexcept;
    [[nodiscard]] double duration_seconds() const noexcept;
    [[nodiscard]] const std::string& error() const noexcept;
    [[nodiscard]] std::string library_label() const;

    void poll();
    void play_track(std::size_t index);
    void toggle_playback();
    void next();
    void previous();
    void seek(double seconds);
    void set_volume(int volume);
    void toggle_shuffle();
    void cycle_repeat();
    void toggle_favorite();
    void rescan();

    [[nodiscard]] static std::vector<MusicTrack> scan_directory(const std::string& path);

private:
    struct ScanResult {
        std::vector<MusicTrack> tracks;
        std::string error;
    };
    void advance(bool automatic);
    void start_track(std::size_t index, bool add_history);
    void rebuild_shuffle();
    void save_preferences() const;
    void apply_preferences();
    [[nodiscard]] static ScanResult scan_library(const std::shared_ptr<std::atomic_bool>& cancelled);

    std::unique_ptr<AudioBackend> backend_;
    std::vector<MusicTrack> tracks_;
    std::size_t current_index_ = 0;
    AudioSnapshot playback_;
    bool shuffle_             = false;
    RepeatMode repeat_        = RepeatMode::all;
    int volume_               = 65;
    bool finished_handled_    = false;
    bool pause_requested_     = false;
    bool scanning_            = false;
    bool preferences_enabled_ = false;
    std::string scan_error_;
    std::string preferences_path_;
    std::vector<std::string> favorite_paths_;
    std::vector<std::size_t> shuffle_queue_;
    std::vector<std::size_t> history_;
    std::mt19937 random_{std::random_device{}()};
    std::shared_ptr<std::atomic_bool> scan_cancelled_;
    std::future<ScanResult> scan_;
};

#endif
