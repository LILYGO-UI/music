#pragma once

#include "components/lv_subject.hpp"
#include "pages/music/music_model.hpp"
#include <string>
#include <vector>

class MusicViewModel {
public:
    MusicViewModel();
    explicit MusicViewModel(std::unique_ptr<AudioBackend> backend, std::vector<MusicTrack> tracks);
    const MusicModel &model() const noexcept
    {
        return model_;
    }
    lv_subject_t *state_subject() noexcept
    {
        return revision_.get();
    }
    std::vector<std::size_t> visible_tracks() const;
    bool favorites_only() const noexcept
    {
        return favorites_only_;
    }
    const std::string &query() const noexcept
    {
        return query_;
    }
    void poll();
    void play(std::size_t index);
    void toggle_playback();
    void next();
    void previous();
    void seek(int permille);
    void set_volume(int volume);
    void toggle_shuffle();
    void cycle_repeat();
    void toggle_favorite();
    void set_favorites_only(bool value);
    void set_query(std::string value);
    void rescan();
    static std::string format_time(double seconds);

private:
    void publish();
    MusicModel model_;
    IntSubject revision_{0};
    bool favorites_only_ = false;
    std::string query_;
};
