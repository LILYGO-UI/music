#include "pages/music/music_view.hpp"
#include <cm0/status_bar.h>
#include <cm0/typography.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kPartialRows = 80;
std::vector<lv_color32_t> framebuffer;
std::vector<lv_color32_t> draw_buffer;
int display_width      = 0;
int display_height     = 0;
const char *test_phase = "initial";

class FakeAudioBackend final : public AudioBackend {
public:
    AudioSnapshot state;
    int volume = 65;
    void load(const std::string &path, int value) override
    {
        state        = {};
        state.path   = path;
        state.loaded = state.playing = true;
        state.duration_seconds       = 32;
        volume                       = value;
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
            {"glow", "/fixtures/glow.wav", "Afterglow", "LILYGO Sessions", "Little Moments", 32, false, true, 2}};
}

void flush_display(lv_display_t *display, const lv_area_t *area, std::uint8_t *pixels)
{
    const int width    = area->x2 - area->x1 + 1;
    const auto *source = reinterpret_cast<const lv_color32_t *>(pixels);
    assert(area->x1 >= 0 && area->y1 >= 0 && area->x2 < display_width && area->y2 < display_height);
    for (int y = area->y1; y <= area->y2; ++y) {
        const auto source_offset      = static_cast<std::size_t>(y - area->y1) * width;
        const auto destination_offset = static_cast<std::size_t>(y) * display_width + area->x1;
        std::copy_n(source + source_offset, width, framebuffer.begin() + destination_offset);
    }
    lv_display_flush_ready(display);
}

void resize_display(lv_display_t *display, int width, int height)
{
    display_width  = width;
    display_height = height;
    framebuffer.assign(static_cast<std::size_t>(width) * height, {});
    draw_buffer.resize(static_cast<std::size_t>(width) * kPartialRows);
    lv_display_set_buffers(display, draw_buffer.data(), nullptr,
                           static_cast<std::uint32_t>(draw_buffer.size() * sizeof(lv_color32_t)),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_resolution(display, width, height);
    lv_obj_update_layout(lv_screen_active());
}

void refresh(lv_display_t *display)
{
    // A size event can update flex constraints during the first layout pass.
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(display);
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(display);
}

bool write_ppm(const char *path)
{
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file << "P6\n" << display_width << ' ' << display_height << "\n255\n";
    for (const auto &pixel : framebuffer) {
        file.put(static_cast<char>(pixel.red));
        file.put(static_cast<char>(pixel.green));
        file.put(static_cast<char>(pixel.blue));
    }
    return file.good();
}

lv_obj_t *find(lv_obj_t *root, const char *name)
{
    auto *object = lv_obj_find_by_name(root, name);
    if (!object) std::cerr << "Missing UI object: " << name << '\n';
    assert(object);
    return object;
}

void click(lv_obj_t *root, const char *name)
{
    lv_obj_send_event(find(root, name), LV_EVENT_CLICKED, nullptr);
    lv_obj_update_layout(root);
}

void dump_ancestors(lv_obj_t *object)
{
    for (auto *ancestor = object; ancestor; ancestor = lv_obj_get_parent(ancestor)) {
        lv_area_t bounds;
        lv_obj_get_coords(ancestor, &bounds);
        const auto *name = lv_obj_get_name(ancestor);
        std::cerr << "  " << (name ? name : "unnamed") << " bounds " << bounds.x1 << ',' << bounds.y1 << " to "
                  << bounds.x2 << ',' << bounds.y2 << "; content " << lv_obj_get_content_width(ancestor) << 'x'
                  << lv_obj_get_content_height(ancestor) << "; padding top/right/bottom/left "
                  << lv_obj_get_style_pad_top(ancestor, LV_PART_MAIN) << '/'
                  << lv_obj_get_style_pad_right(ancestor, LV_PART_MAIN) << '/'
                  << lv_obj_get_style_pad_bottom(ancestor, LV_PART_MAIN) << '/'
                  << lv_obj_get_style_pad_left(ancestor, LV_PART_MAIN) << '\n';
    }
}

void assert_visible(lv_obj_t *root, const char *name)
{
    auto *object = find(root, name);
    lv_area_t bounds;
    lv_obj_get_coords(object, &bounds);
    if (!lv_obj_is_visible(object)) {
        std::cerr << "UI object is not visible during " << test_phase << ": " << name << '\n';
        dump_ancestors(object);
    }
    assert(lv_obj_is_visible(object));
    const bool contained = bounds.x1 >= 0 && bounds.y1 >= 0 && bounds.x2 < display_width && bounds.y2 < display_height;
    if (!contained) {
        std::cerr << name << " is outside viewport during " << test_phase << '\n';
        dump_ancestors(object);
    }
    assert(contained);
    assert(lv_area_get_width(&bounds) > 0 && lv_area_get_height(&bounds) > 0);
    for (auto *ancestor = lv_obj_get_parent(object); ancestor; ancestor = lv_obj_get_parent(ancestor)) {
        if (lv_obj_has_flag(ancestor, LV_OBJ_FLAG_OVERFLOW_VISIBLE)) continue;
        lv_area_t clip;
        lv_obj_get_coords(ancestor, &clip);
        const bool unclipped =
            bounds.x1 >= clip.x1 && bounds.y1 >= clip.y1 && bounds.x2 <= clip.x2 && bounds.y2 <= clip.y2;
        if (!unclipped) {
            std::cerr << name << " is clipped by its parent during " << test_phase << '\n';
            dump_ancestors(object);
        }
        assert(unclipped);
    }
}

void assert_player_visible(lv_obj_t *root, bool volume_visible = true)
{
    for (const char *name : {"music_player", "music_cover", "music_title", "music_favorite", "music_play",
                             "music_previous", "music_next", "music_progress", "music_shuffle", "music_repeat"})
        assert_visible(root, name);
    if (volume_visible) assert_visible(root, "music_volume");
}

void assert_library_visible(lv_obj_t *root)
{
    for (const char *name : {"music_library", "music_all", "music_favorites", "music_search", "music_track_list"})
        assert_visible(root, name);
}

}  // namespace

int main(int argc, char **argv)
{
    assert(argc == 4);
    const int width  = std::atoi(argv[1]);
    const int height = std::atoi(argv[2]);
    assert(width >= 320 && height >= 240);
    lv_init();
    auto *display = lv_display_create(width, height);
    assert(display);
    lv_display_set_default(display);
    lv_display_set_flush_cb(display, flush_display);
    resize_display(display, width, height);
    auto *root = lv_screen_active();
    {
        auto backend = std::make_unique<FakeAudioBackend>();
        auto *audio  = backend.get();
        MusicViewModel vm(std::move(backend), tracks());
        MusicView view(vm);
        auto *surface = view.create(root, "Music");
        lv_obj_update_layout(root);
        auto *play     = find(root, "music_play");
        auto *progress = find(root, "music_progress");
        assert(std::string(lv_label_get_text(find(root, "music_title"))) == "Soft Landing");
        assert(!vm.model().playing() && vm.model().position_seconds() == 0);
        assert(lv_slider_get_value(progress) == 0);
        // Capture the honest initial paused state before exercising any commands.
        lv_tick_inc(20);
        refresh(display);
        assert(std::any_of(framebuffer.cbegin() + 1, framebuffer.cend(), [](const auto &pixel) {
            return pixel.red != framebuffer[0].red || pixel.green != framebuffer[0].green ||
                   pixel.blue != framebuffer[0].blue;
        }));
        assert(write_ppm(argv[3]));
        assert_player_visible(root);
        if (width > height) assert_library_visible(root);

        click(root, "music_play");
        vm.poll();
        assert(vm.model().playing() && audio->state.path == "/fixtures/soft.wav");
        click(root, "music_play");
        vm.poll();
        assert(!vm.model().playing());
        click(root, "music_next");
        vm.poll();
        assert(vm.model().current_index() == 1);
        assert(std::string(lv_label_get_text(find(root, "music_title"))) == "Blue Hour");
        click(root, "music_previous");
        vm.poll();
        assert(vm.model().current_index() == 0);
        click(root, "music_favorite");
        assert(vm.model().current_track()->favorite);
        lv_slider_set_value(progress, 500, LV_ANIM_OFF);
        lv_obj_send_event(progress, LV_EVENT_RELEASED, nullptr);
        vm.poll();
        assert(std::abs(vm.model().position_seconds() - 16) < 0.001);
        auto *volume = find(root, "music_volume");
        lv_slider_set_value(volume, 37, LV_ANIM_OFF);
        lv_obj_send_event(volume, LV_EVENT_VALUE_CHANGED, nullptr);
        assert(vm.model().volume() == 37 && audio->volume == 37);
        click(root, "music_shuffle");
        assert(vm.model().shuffle());
        const auto repeat = vm.model().repeat_mode();
        click(root, "music_repeat");
        assert(vm.model().repeat_mode() != repeat);

        if (width < height) click(root, "music_library_tab");
        assert_library_visible(root);
        click(root, "music_favorites");
        assert(vm.favorites_only() && vm.visible_tracks() == (std::vector<std::size_t>{0}));
        click(root, "music_all");
        auto *search = find(root, "music_search");
        lv_textarea_set_text(search, "bLuE");
        assert(vm.visible_tracks() == (std::vector<std::size_t>{1}));
        auto *list = find(root, "music_track_list");
        lv_obj_send_event(lv_obj_get_child(list, 0), LV_EVENT_CLICKED, nullptr);
        vm.poll();
        assert(vm.model().current_index() == 1);
        lv_textarea_set_text(search, "no such song");
        assert(vm.visible_tracks().empty());
        lv_textarea_set_text(search, "");
        assert(vm.visible_tracks().size() == 3);
        assert(lv_obj_has_flag(lv_obj_get_child(list, 0), LV_OBJ_FLAG_CLICKABLE));
        lv_obj_send_event(search, LV_EVENT_FOCUSED, nullptr);
        assert(lv_obj_is_visible(find(root, "music_keyboard")));
        lv_obj_send_event(find(root, "music_keyboard"), LV_EVENT_READY, nullptr);
        assert(lv_obj_has_flag(find(root, "music_keyboard"), LV_OBJ_FLAG_HIDDEN));
        // Restore the player before checking the same widgets through rotation.
        if (width < height) click(root, "music_player_tab");
        const auto index       = vm.model().current_index();
        const auto playing     = vm.model().playing();
        const auto child_count = lv_obj_get_child_count(surface);
        test_phase             = "rotated display";
        resize_display(display, height, width);
        assert(find(root, "app_surface") == surface && find(root, "music_play") == play);
        assert(find(root, "music_progress") == progress);
        assert(lv_obj_get_child_count(surface) == child_count);
        assert(vm.model().current_index() == index && vm.model().playing() == playing);
        assert(vm.model().volume() == 37 && vm.model().shuffle());
        assert_player_visible(root);
        lv_refr_now(display);
        test_phase = "restored display";
        resize_display(display, width, height);
        assert_player_visible(root);
        assert(vm.model().tracks()[0].favorite);
        view.destroy();
        view.destroy();
        assert(lv_obj_get_child_count(root) == 0);
        // Published state and timer processing must be safe after teardown.
        vm.set_volume(41);
        lv_tick_inc(250);
        lv_timer_handler();
        assert(lv_obj_get_child_count(root) == 0);
        view.create(root, "Music");
        lv_obj_update_layout(root);
        assert(lv_slider_get_value(find(root, "music_volume")) == 41);
        view.destroy();
    }
    {
        test_phase       = "long Chinese title";
        auto fixture     = tracks();
        fixture[0].title = "在城市的夜色里慢慢听见心中的温柔与远方 — 一首很长的中文歌曲名字";
        MusicViewModel vm(std::make_unique<FakeAudioBackend>(), fixture);
        MusicView view(vm);
        view.create(root, "Music");
        lv_obj_update_layout(root);
        // LV_LABEL_LONG_DOT may replace the label's tail while preserving the
        // model's complete metadata; verify the text survives without overflow.
        const std::string rendered_title = lv_label_get_text(find(root, "music_title"));
        assert(vm.model().current_track()->title == fixture[0].title);
        assert(!rendered_title.empty() && rendered_title.compare(0, 3, fixture[0].title, 0, 3) == 0);
        assert_player_visible(root);
        lv_refr_now(display);
        view.destroy();
    }
    {
        test_phase = "empty library";
        MusicViewModel empty(std::make_unique<FakeAudioBackend>(), {});
        MusicView view(empty);
        view.create(root, "Music");
        lv_obj_update_layout(root);
        assert(lv_obj_has_state(find(root, "music_play"), LV_STATE_DISABLED));
        assert(lv_obj_has_state(find(root, "music_progress"), LV_STATE_DISABLED));
        assert(lv_obj_has_state(find(root, "music_favorite"), LV_STATE_DISABLED));
        click(root, "music_next");
        click(root, "music_previous");
        assert(!empty.model().current_track() && !empty.model().playing());
        if (width < height) click(root, "music_library_tab");
        assert_library_visible(root);
        auto *list = find(root, "music_track_list");
        assert(lv_obj_get_child_count(list) > 0);
        assert(!lv_obj_has_flag(lv_obj_get_child(list, 0), LV_OBJ_FLAG_CLICKABLE));
        lv_refr_now(display);
        view.destroy();
        view.destroy();
    }
    {
        test_phase = "AppKit status-bar container";
        // Mirror runtime.cpp:create_app_root: retain lv_obj_create's default
        // horizontal/bottom padding while reserving the real status-bar height.
        auto *content = lv_obj_create(root);
        lv_obj_set_name_static(content, "runtime_content");
        lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
        lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_pad_top(content, CM0_STATUS_BAR_HEIGHT, 0);
        lv_obj_set_style_border_width(content, 0, 0);
        lv_obj_set_style_radius(content, 0, 0);
        lv_obj_set_style_bg_color(content, lv_color_hex(0xf2f2f7), 0);
        lv_obj_set_style_bg_grad_dir(content, LV_GRAD_DIR_NONE, 0);
        lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
        lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        auto backend = std::make_unique<FakeAudioBackend>();
        auto *audio  = backend.get();
        MusicViewModel vm(std::move(backend), tracks());
        MusicView view(vm);
        view.create(content, "Music");
        refresh(display);
        const std::string runtime_path = std::string(argv[3]) + ".appkit.ppm";
        assert(write_ppm(runtime_path.c_str()));
        assert_player_visible(root);
        if (width > height) assert_library_visible(root);
        click(root, "music_play");
        vm.poll();
        assert(vm.model().playing());
        click(root, "music_next");
        vm.poll();
        assert(vm.model().current_index() == 1);
        assert_player_visible(root);
        test_phase           = "AppKit status-bar container with audio error";
        audio->state.path    = vm.model().current_track()->path;
        audio->state.playing = false;
        audio->state.error   = "无法播放此文件，请检查文件或音频设备";
        vm.poll();
        refresh(display);
        const std::string error_path = std::string(argv[3]) + ".error.ppm";
        assert(write_ppm(error_path.c_str()));
        assert_visible(root, "music_error");
        assert(std::string(lv_label_get_text(find(root, "music_error"))) == audio->state.error);
        assert_player_visible(root, width < height);
        assert(!vm.model().playing());
        audio->state.error.clear();
        vm.poll();
        refresh(display);
        assert(lv_obj_has_flag(find(root, "music_error"), LV_OBJ_FLAG_HIDDEN));
        assert_player_visible(root);
        view.destroy();
        lv_obj_delete(content);
    }
    lv_display_delete(display);
    lilygo_ui_fonts_deinit();
    lv_deinit();
    return 0;
}
