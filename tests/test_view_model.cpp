#include "pages/music/music_view_model.hpp"
#include <cm0/typography.h>

#include <cassert>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

class FakeAudioBackend final : public AudioBackend {
public:
    AudioSnapshot state;
    int volume = 65;
    int loads  = 0;
    void load(const std::string &path, int value) override
    {
        state        = {};
        state.path   = path;
        state.loaded = state.playing = true;
        state.duration_seconds       = 32;
        volume                       = value;
        ++loads;
    }
    void set_paused(bool paused) override
    {
        state.playing = !paused;
    }
    void seek(double seconds) override
    {
        state.position_seconds = seconds;
    }
    void set_volume(int value) override
    {
        volume = value;
    }
    void stop() override
    {
        state = {};
    }
    AudioSnapshot snapshot() const override
    {
        return state;
    }
};

std::vector<MusicTrack> tracks()
{
    return {{"soft", "/fixtures/soft.wav", "Soft Landing", "LILYGO Sessions", "Little Moments", 32, false, true, 0},
            {"blue", "/fixtures/blue.wav", "Blue Hour", "LILYGO Sessions", "Little Moments", 32, false, true, 1},
            {"glow", "/fixtures/glow.wav", "Afterglow", "Evening Ensemble", "夜色", 32, false, true, 2}};
}

void notification(lv_observer_t *observer, lv_subject_t *)
{
    ++*static_cast<int *>(lv_observer_get_user_data(observer));
}

}  // namespace

int main()
{
    lv_init();
    {
        auto backend = std::make_unique<FakeAudioBackend>();
        auto *audio  = backend.get();
        MusicViewModel vm(std::move(backend), tracks());
        int notifications               = 0;
        auto *observer                  = lv_subject_add_observer(vm.state_subject(), notification, &notifications);
        const int initial_notifications = notifications;
        assert(lv_subject_get_int(vm.state_subject()) == 0);
        assert(vm.visible_tracks() == (std::vector<std::size_t>{0, 1, 2}));
        assert(!vm.model().playing());
        assert(vm.model().position_seconds() == 0);
        assert(vm.model().duration_seconds() == 32);

        vm.play(1);
        vm.poll();
        assert(audio->state.path == "/fixtures/blue.wav");
        assert(vm.model().current_index() == 1 && vm.model().playing());
        assert(notifications > initial_notifications);
        assert(lv_subject_get_int(vm.state_subject()) > 0);
        vm.toggle_playback();
        vm.poll();
        assert(!vm.model().playing() && !audio->state.playing);
        vm.toggle_playback();
        vm.poll();
        assert(vm.model().playing());

        vm.seek(500);
        vm.poll();
        assert(std::abs(audio->state.position_seconds - 16) < 0.001);
        vm.seek(-1);
        vm.poll();
        assert(audio->state.position_seconds == 0);
        vm.seek(1500);
        vm.poll();
        assert(audio->state.position_seconds <= 32 && audio->state.position_seconds >= 31);
        vm.seek(0);
        vm.next();
        vm.poll();
        assert(vm.model().current_index() == 2);
        vm.previous();
        vm.poll();
        assert(vm.model().current_index() == 1);
        vm.seek(500);
        vm.previous();
        vm.poll();
        assert(vm.model().current_index() == 1 && vm.model().position_seconds() == 0);

        vm.set_volume(120);
        assert(vm.model().volume() == 100 && audio->volume == 100);
        vm.set_volume(-15);
        assert(vm.model().volume() == 0 && audio->volume == 0);
        vm.set_volume(42);
        assert(vm.model().volume() == 42 && audio->volume == 42);
        const bool original_shuffle = vm.model().shuffle();
        vm.toggle_shuffle();
        assert(vm.model().shuffle() != original_shuffle);
        vm.toggle_shuffle();
        assert(vm.model().shuffle() == original_shuffle);
        const auto original_repeat = vm.model().repeat_mode();
        vm.cycle_repeat();
        assert(vm.model().repeat_mode() != original_repeat);
        vm.cycle_repeat();
        assert(vm.model().repeat_mode() != original_repeat);
        vm.cycle_repeat();
        assert(vm.model().repeat_mode() == original_repeat);

        vm.set_favorites_only(true);
        assert(vm.favorites_only() && vm.visible_tracks().empty());
        vm.toggle_favorite();
        assert(vm.model().current_track()->favorite);
        assert(vm.visible_tracks() == (std::vector<std::size_t>{1}));
        vm.set_query("bLuE");
        assert(vm.query() == "bLuE" && vm.visible_tracks() == (std::vector<std::size_t>{1}));
        vm.set_query("Soft");
        assert(vm.visible_tracks().empty());
        vm.set_favorites_only(false);
        assert(vm.visible_tracks() == (std::vector<std::size_t>{0}));
        vm.set_query("LILYGO");
        assert(vm.visible_tracks() == (std::vector<std::size_t>{0, 1}));
        vm.set_query("little moments");
        assert(vm.visible_tracks() == (std::vector<std::size_t>{0, 1}));
        vm.set_query("夜色");
        assert(vm.visible_tracks() == (std::vector<std::size_t>{2}));
        vm.set_query("absent song");
        assert(vm.visible_tracks().empty());
        vm.set_query("");
        assert(vm.visible_tracks().size() == 3);
        vm.toggle_favorite();
        assert(!vm.model().current_track()->favorite);

        const int loads = audio->loads;
        vm.play(999);
        assert(audio->loads == loads && vm.model().current_index() == 1);
        audio->state.position_seconds = 7.25;
        vm.poll();
        assert(vm.model().position_seconds() == 7.25);
        audio->state.error   = "Audio device unavailable";
        audio->state.playing = false;
        vm.poll();
        assert(vm.model().error() == "Audio device unavailable");
        assert(!vm.model().playing());
        lv_observer_remove(observer);
    }
    {
        auto backend = std::make_unique<FakeAudioBackend>();
        auto *audio  = backend.get();
        MusicViewModel empty(std::move(backend), {});
        empty.toggle_playback();
        empty.next();
        empty.previous();
        empty.seek(500);
        empty.play(0);
        empty.toggle_favorite();
        empty.poll();
        assert(empty.visible_tracks().empty());
        assert(!empty.model().current_track() && !empty.model().playing());
        assert(audio->loads == 0);
    }
    assert(MusicViewModel::format_time(0) == "0:00");
    assert(MusicViewModel::format_time(65.9) == "1:05");
    assert(MusicViewModel::format_time(3600) == "60:00");
    assert(MusicViewModel::format_time(-1) == "0:00");
    assert(MusicViewModel::format_time(std::numeric_limits<double>::infinity()) == "0:00");
    assert(MusicViewModel::format_time(std::numeric_limits<double>::quiet_NaN()) == "0:00");
    assert(MusicViewModel::format_time(1e20) == "5999:59");
    lilygo_ui_fonts_deinit();
    lv_deinit();
    return 0;
}
