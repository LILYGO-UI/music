#include "pages/music/music_view.hpp"
#include "components/components.hpp"
#include <cm0/typography.h>
#include <algorithm>
#include <cstdint>
#include <sstream>

namespace {
// Music-specific semantic colors: coral actions, muted secondary text, subtle selected surfaces.
constexpr uint32_t accent    = 0xfa365d;
constexpr uint32_t secondary = 0x898991;
constexpr uint32_t selected  = 0xffedf1;
constexpr uint32_t ink       = components::color_container_title;
constexpr uint32_t white     = components::color_container_background;

void font(lv_obj_t *object, int size)
{
    if (const auto *value = lilygo_ui_font_get(size)) lv_obj_set_style_text_font(object, value, 0);
}
lv_obj_t *box(lv_obj_t *parent, lv_flex_flow_t flow, int gap = 0)
{
    auto *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_size(object, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(object, flow);
    lv_obj_set_style_pad_gap(object, gap, 0);
    lv_obj_remove_flag(object, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    return object;
}
lv_obj_t *label(lv_obj_t *parent, const char *text, int size, uint32_t color = ink)
{
    auto *object = lv_label_create(parent);
    lv_label_set_text(object, text);
    font(object, size);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    return object;
}
void flexible(lv_obj_t *object)
{
    lv_obj_set_width(object, 0);
    lv_obj_set_flex_grow(object, 1);
}
void text_button(lv_obj_t *object, const char *text)
{
    lv_label_set_text(lv_obj_get_child(object, 0), text);
}
void tint_button(lv_obj_t *object, bool active)
{
    lv_obj_set_style_bg_color(object, lv_color_hex(active ? selected : white), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(object, 0), lv_color_hex(active ? accent : secondary), 0);
}
void ellipsis(lv_obj_t *object)
{
    lv_obj_set_width(object, LV_PCT(100));
    lv_label_set_long_mode(object, LV_LABEL_LONG_DOT);
    if (const auto *text_font = lv_obj_get_style_text_font(object, LV_PART_MAIN))
        lv_obj_set_height(object, lv_font_get_line_height(text_font));
}
void slider_style(lv_obj_t *slider, uint32_t color)
{
    lv_obj_set_height(slider, 5);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xe8e8ed), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(color), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 5, LV_PART_KNOB);
    lv_obj_set_ext_click_area(slider, 18);
}
}  // namespace

lv_obj_t *MusicView::button(lv_obj_t *parent, const char *text, Action action, const char *name, int width)
{
    auto *object = lv_button_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_size(object, width, 48);
    lv_obj_set_style_radius(object, 14, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(white), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(selected), LV_STATE_PRESSED);
    auto *caption = label(object, text, 22, accent);
    lv_obj_center(caption);
    lv_obj_set_name_static(object, name);
    auto command = std::make_unique<Command>(Command{this, action});
    lv_obj_add_event_cb(object, command_event, LV_EVENT_CLICKED, command.get());
    commands_.push_back(std::move(command));
    return object;
}

lv_obj_t *MusicView::artwork(lv_obj_t *parent, int size, unsigned theme)
{
    auto *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_set_size(object, size, size);
    lv_obj_set_style_radius(object, size > 100 ? 18 : 10, 0);
    lv_obj_set_style_clip_corner(object, true, 0);
    lv_obj_remove_flag(object, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_user_data(object, reinterpret_cast<void *>(static_cast<uintptr_t>(theme)));
    // Put the illustration in a child so the rounded parent clips every draw task.
    auto *painting = lv_obj_create(object);
    lv_obj_remove_style_all(painting);
    lv_obj_set_size(painting, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(painting, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_event_cb(painting, art_draw, LV_EVENT_DRAW_MAIN, nullptr);
    return object;
}

void MusicView::art_draw(lv_event_t *event)
{
    auto *object = lv_event_get_target_obj(event);
    auto *layer  = lv_event_get_layer(event);
    lv_area_t bounds;
    lv_obj_get_coords(object, &bounds);
    const int size              = lv_area_get_width(&bounds);
    const auto theme            = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(lv_obj_get_parent(object))) % 3;
    const uint32_t skies[]      = {0x224b4b, 0x343e67, 0x8a4650};
    const uint32_t suns[]       = {0xf1bc8d, 0xd0d7ec, 0xf7c18f};
    const uint32_t hills[]      = {0x163b3e, 0x252f52, 0x663642};
    const uint32_t foreground[] = {0x0b2e34, 0x1a2241, 0x442a39};
    auto rect                   = [&](int x, int y, int width, int height, uint32_t color, int radius) {
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = lv_color_hex(color);
        dsc.bg_opa   = LV_OPA_COVER;
        dsc.radius   = radius;
        lv_area_t area{bounds.x1 + x, bounds.y1 + y, bounds.x1 + x + width - 1, bounds.y1 + y + height - 1};
        lv_draw_rect(layer, &dsc, &area);
    };
    rect(0, 0, size, size, skies[theme], size > 100 ? 18 : 10);
    rect(size * 30 / 100, size * 19 / 100, size * 49 / 100, size * 49 / 100, suns[theme], LV_RADIUS_CIRCLE);
    // Layered, intentionally geometric landscape; decorative coordinates scale with the artwork.
    rect(-size / 3, size * 54 / 100, size * 12 / 10, size, hills[theme], size / 2);
    rect(size / 4, size * 64 / 100, size, size, foreground[theme], size / 2);
    if (size > 140) {
        lv_draw_label_dsc_t text;
        lv_draw_label_dsc_init(&text);
        text.font = lilygo_ui_font_get(14);
        if (!text.font) return;
        text.color = lv_color_hex(0xf8e8d4);
        text.text  = size >= 280 ? "L I L Y G O   /   S E S S I O N S" : "L I L Y G O";
        lv_area_t area{bounds.x1 + size / 12, bounds.y1 + size / 12, bounds.x2 - size / 12, bounds.y2};
        lv_draw_label(layer, &text, &area);
        text.font = lilygo_ui_font_get(size > 320 ? 48 : 28);
        if (!text.font) return;
        text.text = theme == 0 ? "little\nmoments." : (theme == 1 ? "blue\nhour." : "after\nglow.");
        area.y1   = bounds.y1 + size * 61 / 100;
        lv_draw_label(layer, &text, &area);
    }
}

lv_obj_t *MusicView::create(lv_obj_t *parent, const char *)
{
    surface_ = components::create_page(parent);
    lv_obj_set_style_pad_all(surface_, 24, 0);
    lv_obj_set_style_pad_gap(surface_, 18, 0);
    header_ = box(surface_, LV_FLEX_FLOW_ROW, 12);
    lv_obj_set_name_static(header_, "app_header");
    lv_obj_set_height(header_, 60);
    lv_obj_set_flex_align(header_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    auto *brand = label(header_, LV_SYMBOL_AUDIO, 28, accent);
    lv_obj_set_width(brand, 36);
    auto *title = label(header_, "Music", 36);
    lv_obj_set_name_static(title, "app_title");
    flexible(title);

    tabs_ = box(surface_, LV_FLEX_FLOW_ROW, 8);
    lv_obj_set_height(tabs_, 52);
    lv_obj_set_style_pad_all(tabs_, 4, 0);
    lv_obj_set_style_bg_color(tabs_, lv_color_hex(0xe6e6ed), 0);
    lv_obj_set_style_bg_opa(tabs_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tabs_, 16, 0);
    player_tab_  = button(tabs_, "Now Playing", Action::player, "music_player_tab", 100);
    library_tab_ = button(tabs_, "Library", Action::library, "music_library_tab", 100);
    for (auto *tab : {player_tab_, library_tab_}) {
        flexible(tab);
        lv_obj_set_height(tab, 44);
        font(lv_obj_get_child(tab, 0), 18);
    }

    body_ = box(surface_, LV_FLEX_FLOW_ROW, 20);
    lv_obj_set_height(body_, 0);
    lv_obj_set_flex_grow(body_, 1);
    library_ = box(body_, LV_FLEX_FLOW_COLUMN, 16);
    lv_obj_set_name_static(library_, "music_library");
    lv_obj_set_height(library_, LV_PCT(100));
    lv_obj_set_style_bg_color(library_, lv_color_hex(white), 0);
    lv_obj_set_style_bg_opa(library_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(library_, 24, 0);
    lv_obj_set_style_pad_all(library_, 24, 0);
    auto *library_heading = box(library_, LV_FLEX_FLOW_ROW, 12);
    lv_obj_set_flex_align(library_heading, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    auto *library_title = label(library_heading, "Library", 28);
    flexible(library_title);
    library_count_    = label(library_heading, "", 14, secondary);
    button(library_heading, LV_SYMBOL_REFRESH, Action::rescan, "music_rescan");
    auto *filters     = box(library_, LV_FLEX_FLOW_ROW, 12);
    all_button_       = button(filters, LV_SYMBOL_AUDIO "  All Songs", Action::all, "music_all", 150);
    favorites_button_ = button(filters, "\xef\x80\x84  Favorites", Action::favorites, "music_favorites", 140);
    font(lv_obj_get_child(all_button_, 0), 18);
    font(lv_obj_get_child(favorites_button_, 0), 18);
    search_ = lv_textarea_create(library_);
    lv_obj_set_name_static(search_, "music_search");
    lv_obj_set_size(search_, LV_PCT(100), 48);
    lv_textarea_set_one_line(search_, true);
    lv_textarea_set_placeholder_text(search_, "Search songs or artists");
    font(search_, 18);
    lv_obj_set_style_border_width(search_, 0, 0);
    lv_obj_set_style_bg_color(search_, lv_color_hex(components::color_page_background), 0);
    lv_obj_set_style_radius(search_, 12, 0);
    lv_obj_add_event_cb(search_, search_event, LV_EVENT_ALL, this);
    list_ = box(library_, LV_FLEX_FLOW_COLUMN, 8);
    lv_obj_set_name_static(list_, "music_track_list");
    lv_obj_set_height(list_, 0);
    lv_obj_set_flex_grow(list_, 1);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_AUTO);
    library_footer_ = label(library_, "Searching for music…", 14, secondary);
    ellipsis(library_footer_);

    player_ = box(body_, LV_FLEX_FLOW_COLUMN, 16);
    lv_obj_set_name_static(player_, "music_player");
    lv_obj_set_height(player_, LV_PCT(100));
    lv_obj_set_style_bg_color(player_, lv_color_hex(white), 0);
    lv_obj_set_style_bg_opa(player_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(player_, 24, 0);
    lv_obj_set_style_pad_all(player_, 28, 0);
    player_main_ = box(player_, LV_FLEX_FLOW_COLUMN, 28);
    lv_obj_set_height(player_main_, 0);
    lv_obj_set_flex_grow(player_main_, 1);
    art_area_ = box(player_main_, LV_FLEX_FLOW_COLUMN, 20);
    lv_obj_set_flex_align(art_area_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    cover_ = artwork(art_area_, 240, 0);
    lv_obj_set_name_static(cover_, "music_cover");
    lv_obj_set_style_shadow_width(cover_, 28, 0);
    lv_obj_set_style_shadow_opa(cover_, LV_OPA_10, 0);
    lv_obj_set_style_shadow_offset_y(cover_, 10, 0);
    source_ = label(art_area_, "LILYGO SESSIONS", 14, secondary);
    lv_obj_set_style_text_letter_space(source_, 2, 0);

    controls_ = box(player_main_, LV_FLEX_FLOW_COLUMN, 20);
    lv_obj_set_flex_align(controls_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    auto *metadata = box(controls_, LV_FLEX_FLOW_ROW, 12);
    lv_obj_set_flex_align(metadata, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    auto *titles = box(metadata, LV_FLEX_FLOW_COLUMN, 7);
    flexible(titles);
    song_title_ = label(titles, "Your music, anytime", 28);
    ellipsis(song_title_);
    lv_obj_set_name_static(song_title_, "music_title");
    artist_ = label(titles, "Pick a song and start listening", 18, accent);
    ellipsis(artist_);
    album_ = label(titles, "", 14, secondary);
    ellipsis(album_);
    favorite_      = button(metadata, "\xef\x80\x84", Action::favorite, "music_favorite", 44);
    auto *timeline = box(controls_, LV_FLEX_FLOW_COLUMN, 16);
    lv_obj_set_style_pad_top(timeline, 12, 0);
    // LVGL centers the knob on the indicator edge and expands it by about
    // 7 px. Its pressed state adds another 3 px of transform, so leave 12 px
    // inside the timeline to keep the enlarged endpoint fully visible.
    lv_obj_set_style_pad_left(timeline, 12, 0);
    lv_obj_set_style_pad_right(timeline, 12, 0);
    progress_ = lv_slider_create(timeline);
    lv_obj_set_name_static(progress_, "music_progress");
    lv_obj_set_width(progress_, LV_PCT(100));
    lv_slider_set_range(progress_, 0, 1000);
    slider_style(progress_, 0xadaeb6);
    lv_obj_add_event_cb(progress_, slider_event, LV_EVENT_RELEASED, this);
    auto *time_row = box(timeline, LV_FLEX_FLOW_ROW);
    elapsed_       = label(time_row, "0:00", 14, secondary);
    flexible(elapsed_);
    remaining_      = label(time_row, "−0:00", 14, secondary);
    auto *transport = box(controls_, LV_FLEX_FLOW_ROW, 24);
    lv_obj_set_flex_align(transport, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    auto *previous = button(transport, LV_SYMBOL_PREV, Action::previous, "music_previous", 56);
    play_          = button(transport, LV_SYMBOL_PLAY, Action::play, "music_play", 76);
    lv_obj_set_size(play_, 76, 76);
    lv_obj_set_style_radius(play_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(play_, lv_color_hex(accent), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(play_, 0), lv_color_hex(white), 0);
    font(lv_obj_get_child(play_, 0), 28);
    auto *next = button(transport, LV_SYMBOL_NEXT, Action::next, "music_next", 56);
    for (auto *item : {previous, next}) {
        font(lv_obj_get_child(item, 0), 28);
        lv_obj_set_style_text_color(lv_obj_get_child(item, 0), lv_color_hex(ink), 0);
    }
    auto *volume_row = box(controls_, LV_FLEX_FLOW_ROW, 20);
    lv_obj_set_height(volume_row, 28);
    lv_obj_set_flex_align(volume_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label(volume_row, LV_SYMBOL_VOLUME_MID, 14, secondary);
    volume_ = lv_slider_create(volume_row);
    lv_obj_set_name_static(volume_, "music_volume");
    flexible(volume_);
    lv_slider_set_range(volume_, 0, 100);
    slider_style(volume_, 0xadaeb6);
    lv_obj_add_event_cb(volume_, slider_event, LV_EVENT_VALUE_CHANGED, this);
    label(volume_row, LV_SYMBOL_VOLUME_MAX, 14, secondary);
    auto *modes = box(controls_, LV_FLEX_FLOW_ROW, 12);
    lv_obj_set_flex_align(modes, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    shuffle_ = button(modes, LV_SYMBOL_SHUFFLE, Action::shuffle, "music_shuffle");
    status_  = label(modes, "Ready to play", 14, secondary);
    flexible(status_);
    lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(status_, LV_LABEL_LONG_DOT);
    repeat_ = button(modes, LV_SYMBOL_LOOP, Action::repeat, "music_repeat");

    error_ = label(player_, "", 14, accent);
    lv_obj_set_name_static(error_, "music_error");
    lv_obj_set_width(error_, LV_PCT(100));
    lv_label_set_long_mode(error_, LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(error_, LV_OBJ_FLAG_HIDDEN);

    keyboard_ = lv_keyboard_create(surface_);
    lv_obj_set_name_static(keyboard_, "music_keyboard");
    lv_obj_add_flag(keyboard_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_HIDDEN));
    lv_obj_set_size(keyboard_, LV_PCT(100), 240);
    lv_obj_align(keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);
    font(keyboard_, 18);
    lv_obj_add_event_cb(keyboard_, keyboard_event, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(surface_, size_event, LV_EVENT_SIZE_CHANGED, this);
    lv_obj_add_event_cb(art_area_, size_event, LV_EVENT_SIZE_CHANGED, this);
    lv_subject_add_observer_obj(view_model_.state_subject(), state_changed, surface_, this);
    timer_ = lv_timer_create(
        [](lv_timer_t *timer) { static_cast<MusicView *>(lv_timer_get_user_data(timer))->view_model_.poll(); }, 200,
        this);
    apply_layout();
    update();
    return surface_;
}

void MusicView::destroy() noexcept
{
    if (timer_) lv_timer_delete(timer_);
    timer_ = nullptr;
    if (surface_) lv_obj_delete(surface_);
    surface_ = header_ = tabs_ = library_tab_ = player_tab_ = body_ = library_ = library_count_ = nullptr;
    all_button_ = favorites_button_ = search_ = list_ = library_footer_ = player_ = player_main_ = nullptr;
    art_area_ = cover_ = source_ = controls_ = song_title_ = artist_ = album_ = favorite_ = nullptr;
    progress_ = elapsed_ = remaining_ = play_ = shuffle_ = repeat_ = volume_ = status_ = error_ = keyboard_ = nullptr;
    commands_.clear();
    row_commands_.clear();
    library_key_.clear();
}

void MusicView::command_event(lv_event_t *event)
{
    const auto command = *static_cast<Command *>(lv_event_get_user_data(event));
    auto &view         = *command.view;
    auto &vm           = view.view_model_;
    if (command.track) {
        vm.play(command.index);
        return;
    }
    switch (command.action) {
        case Action::play:
            vm.toggle_playback();
            break;
        case Action::next:
            vm.next();
            break;
        case Action::previous:
            vm.previous();
            break;
        case Action::shuffle:
            vm.toggle_shuffle();
            break;
        case Action::repeat:
            vm.cycle_repeat();
            break;
        case Action::favorite:
            vm.toggle_favorite();
            break;
        case Action::all:
            vm.set_favorites_only(false);
            break;
        case Action::favorites:
            vm.set_favorites_only(true);
            break;
        case Action::library:
            view.show_library(true);
            break;
        case Action::player:
            view.show_library(false);
            break;
        case Action::rescan:
            vm.rescan();
            break;
    }
}
void MusicView::slider_event(lv_event_t *event)
{
    auto &view = *static_cast<MusicView *>(lv_event_get_user_data(event));
    if (lv_event_get_target_obj(event) == view.progress_)
        view.view_model_.seek(lv_slider_get_value(view.progress_));
    else
        view.view_model_.set_volume(lv_slider_get_value(view.volume_));
}
void MusicView::search_event(lv_event_t *event)
{
    auto &view = *static_cast<MusicView *>(lv_event_get_user_data(event));
    if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED)
        view.view_model_.set_query(lv_textarea_get_text(view.search_));
    if (lv_event_get_code(event) == LV_EVENT_FOCUSED || lv_event_get_code(event) == LV_EVENT_CLICKED) {
        lv_keyboard_set_textarea(view.keyboard_, view.search_);
        lv_obj_remove_flag(view.keyboard_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(view.keyboard_);
    }
    if (lv_event_get_code(event) == LV_EVENT_DEFOCUSED) lv_obj_add_flag(view.keyboard_, LV_OBJ_FLAG_HIDDEN);
}
void MusicView::keyboard_event(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_READY && lv_event_get_code(event) != LV_EVENT_CANCEL) return;
    auto &view = *static_cast<MusicView *>(lv_event_get_user_data(event));
    lv_obj_add_flag(view.keyboard_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_state(view.search_, LV_STATE_FOCUSED);
}
void MusicView::size_event(lv_event_t *event)
{
    static_cast<MusicView *>(lv_event_get_user_data(event))->apply_layout();
}
void MusicView::state_changed(lv_observer_t *observer, lv_subject_t *)
{
    static_cast<MusicView *>(lv_observer_get_user_data(observer))->update();
}
void MusicView::show_library(bool show)
{
    library_visible_ = show;
    if (!show) lv_obj_add_flag(keyboard_, LV_OBJ_FLAG_HIDDEN);
    apply_layout();
}

void MusicView::apply_layout()
{
    if (!surface_ || !cover_ || layout_busy_) return;
    layout_busy_          = true;
    const int width       = lv_obj_get_content_width(surface_);
    const int height      = lv_obj_get_content_height(surface_);
    wide_                 = width >= 850 && width > height;
    const bool short_wide = wide_ && lv_obj_get_height(surface_) < 560;
    lv_obj_set_style_pad_all(surface_, (width < 400 || short_wide) ? 16 : 24, 0);
    lv_obj_set_height(header_, short_wide ? 48 : 60);
    lv_obj_set_style_pad_row(surface_, short_wide ? 12 : 18, 0);
    lv_obj_set_size(play_, short_wide ? 64 : 76, short_wide ? 64 : 76);
    for (auto *mode : {shuffle_, repeat_}) lv_obj_set_height(mode, short_wide ? 44 : 52);
    if (short_wide)
        lv_obj_add_flag(album_, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(album_, LV_OBJ_FLAG_HIDDEN);
    if (short_wide && !view_model_.model().error().empty())
        lv_obj_add_flag(lv_obj_get_parent(volume_), LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(lv_obj_get_parent(volume_), LV_OBJ_FLAG_HIDDEN);
    if (wide_) {
        lv_obj_add_flag(tabs_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(library_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(player_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(library_, LV_PCT(35));
        lv_obj_set_flex_grow(library_, 0);
        flexible(player_);
        lv_obj_set_flex_flow(player_main_, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(player_main_, 24, 0);
        flexible(art_area_);
        lv_obj_set_height(art_area_, LV_PCT(100));
        lv_obj_set_width(controls_, LV_PCT(48));
        lv_obj_set_height(controls_, LV_PCT(100));
        lv_obj_set_style_pad_row(controls_, short_wide ? 12 : 16, 0);
        lv_obj_set_style_pad_all(player_, short_wide ? 16 : 24, 0);
    } else {
        lv_obj_remove_flag(tabs_, LV_OBJ_FLAG_HIDDEN);
        if (library_visible_) {
            lv_obj_remove_flag(library_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(player_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(library_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(player_, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_width(library_, LV_PCT(100));
        lv_obj_set_flex_grow(library_, 0);
        lv_obj_set_width(player_, LV_PCT(100));
        lv_obj_set_flex_grow(player_, 0);
        lv_obj_set_flex_flow(player_main_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_width(art_area_, LV_PCT(100));
        lv_obj_set_height(art_area_, 0);
        lv_obj_set_flex_grow(art_area_, 1);
        lv_obj_set_width(controls_, LV_PCT(100));
        lv_obj_set_height(controls_, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_row(controls_, height < 700 ? 16 : 28, 0);
        lv_obj_set_style_pad_all(player_, width < 400 ? 20 : 28, 0);
    }
    lv_obj_set_style_pad_row(library_, short_wide ? 10 : (wide_ ? 12 : 16), 0);
    lv_obj_set_style_pad_all(library_, short_wide ? 16 : 24, 0);
    for (auto *filter : {all_button_, favorites_button_}) lv_obj_set_height(filter, wide_ ? 44 : 48);
    lv_obj_set_height(search_, wide_ ? 44 : 48);
    for (uint32_t i = 0; i < lv_obj_get_child_count(list_); ++i) {
        auto *row = lv_obj_get_child(list_, i);
        lv_obj_set_height(row, wide_ ? 64 : 76);
        lv_obj_set_size(lv_obj_get_child(row, 0), wide_ ? 48 : 56, wide_ ? 48 : 56);
    }
    tint_button(library_tab_, library_visible_);
    tint_button(player_tab_, !library_visible_);
    lv_obj_update_layout(surface_);
    int cover_size = std::min(lv_obj_get_content_width(art_area_), lv_obj_get_content_height(art_area_) - 42);
    cover_size     = std::clamp(cover_size, 80, 440);
    lv_obj_set_size(cover_, cover_size, cover_size);
    // The source label below the artwork participates in the column layout
    // and leaves the square slightly high. Move the artwork down into the
    // visual center while keeping the label in the same card.
    lv_obj_set_style_translate_y(cover_, 18, 0);
    layout_busy_ = false;
}

void MusicView::update_library()
{
    const auto &model  = view_model_.model();
    const auto visible = view_model_.visible_tracks();
    std::ostringstream key;
    key << model.current_index() << ':' << model.playing() << ':' << model.scanning() << ':'
        << view_model_.favorites_only() << ':' << view_model_.query();
    for (const auto &track : model.tracks()) key << '|' << track.id << track.favorite << track.title << track.artist;
    if (key.str() == library_key_) return;
    library_key_     = key.str();
    const int scroll = lv_obj_get_scroll_y(list_);
    // Reuse rows so observer updates never delete the widget dispatching a click.
    while (lv_obj_get_child_count(list_) < visible.size() + 1) {
        auto *row = box(list_, LV_FLEX_FLOW_ROW, 12);
        lv_obj_set_height(row, wide_ ? 64 : 76);
        lv_obj_set_style_pad_all(row, 8, 0);
        lv_obj_set_style_radius(row, 14, 0);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        artwork(row, wide_ ? 48 : 56, 0);
        auto *titles = box(row, LV_FLEX_FLOW_COLUMN, 5);
        flexible(titles);
        auto *title = label(titles, "", 18);
        ellipsis(title);
        auto *artist = label(titles, "", 14, secondary);
        ellipsis(artist);
        label(row, "", 14, accent);
        auto command = std::make_unique<Command>(Command{this, Action::play, 0, true});
        lv_obj_add_event_cb(row, command_event, LV_EVENT_CLICKED, command.get());
        row_commands_.push_back(std::move(command));
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(list_); ++i) {
        auto *row = lv_obj_get_child(list_, i);
        if (i >= visible.size()) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
        const auto index        = visible[i];
        const auto &track       = model.tracks()[index];
        row_commands_[i]->index = index;
        lv_obj_set_user_data(lv_obj_get_child(row, 0), reinterpret_cast<void *>(static_cast<uintptr_t>(track.artwork)));
        lv_obj_invalidate(lv_obj_get_child(row, 0));
        auto *titles = lv_obj_get_child(row, 1);
        lv_label_set_text(lv_obj_get_child(titles, 0), track.title.c_str());
        lv_label_set_text(lv_obj_get_child(titles, 1), track.artist.c_str());
        const bool active = index == model.current_index();
        lv_obj_set_style_bg_color(row, lv_color_hex(active ? selected : white), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_label_set_text(lv_obj_get_child(row, 2), active ? (model.playing() ? LV_SYMBOL_AUDIO : LV_SYMBOL_PLAY)
                                                           : (track.favorite ? "\xef\x80\x84" : ""));
    }
    if (visible.empty()) {
        auto *row = lv_obj_get_child(list_, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(lv_obj_get_child(row, 0), LV_OBJ_FLAG_HIDDEN);
        auto *titles = lv_obj_get_child(row, 1);
        lv_label_set_text(lv_obj_get_child(titles, 0), model.scanning() ? "Searching for music…" : "No songs yet");
        lv_label_set_text(lv_obj_get_child(titles, 1), view_model_.favorites_only()
                                                           ? "Tap the heart to save favorites"
                                                           : "Add music, then tap Refresh");
        lv_label_set_text(lv_obj_get_child(row, 2), "");
    } else {
        for (uint32_t i = 0; i < visible.size(); ++i) {
            auto *row = lv_obj_get_child(list_, i);
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_flag(lv_obj_get_child(row, 0), LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_scroll_to_y(list_, scroll, LV_ANIM_OFF);
    const auto count = std::to_string(model.tracks().size()) + (model.tracks().size() == 1 ? " song" : " songs");
    lv_label_set_text(library_count_, count.c_str());
    lv_label_set_text(library_footer_, model.library_label().c_str());
}

void MusicView::update()
{
    if (!surface_) return;
    const auto &model = view_model_.model();
    const auto *track = model.current_track();
    if (track) {
        lv_label_set_text(song_title_, track->title.c_str());
        lv_label_set_text(artist_, track->artist.c_str());
        lv_label_set_text(album_, track->album.c_str());
        lv_label_set_text(source_, track->sample ? (track->artist == "Espressif ADF" ? "Espressif ADF · CC BY 3.0"
                                                                                     : "Original music · LILYGO")
                                                 : "");
        const auto theme = reinterpret_cast<void *>(static_cast<uintptr_t>(track->artwork));
        if (lv_obj_get_user_data(cover_) != theme) {
            lv_obj_set_user_data(cover_, theme);
            lv_obj_invalidate(cover_);
        }
    } else {
        lv_label_set_text(song_title_, model.scanning() ? "Discovering music…" : "Add your music");
        lv_label_set_text(artist_, "Add songs to your Music folder");
        lv_label_set_text(album_, "MP3 / FLAC / WAV / M4A / OGG");
        lv_label_set_text(source_, "Your music, anytime");
    }
    text_button(play_, model.playing() ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    tint_button(favorite_, track && track->favorite);
    tint_button(shuffle_, model.shuffle());
    tint_button(repeat_, model.repeat_mode() != RepeatMode::off);
    text_button(repeat_, model.repeat_mode() == RepeatMode::one ? "1" : LV_SYMBOL_LOOP);
    tint_button(all_button_, !view_model_.favorites_only());
    tint_button(favorites_button_, view_model_.favorites_only());
    if (!lv_slider_is_dragged(progress_)) {
        const auto duration = model.duration_seconds();
        lv_slider_set_value(progress_, duration > 0 ? static_cast<int>(1000 * model.position_seconds() / duration) : 0,
                            LV_ANIM_OFF);
    }
    if (!lv_slider_is_dragged(volume_)) lv_slider_set_value(volume_, model.volume(), LV_ANIM_OFF);
    lv_label_set_text(elapsed_, MusicViewModel::format_time(model.position_seconds()).c_str());
    lv_label_set_text(remaining_,
                      ("−" + MusicViewModel::format_time(model.duration_seconds() - model.position_seconds())).c_str());
    lv_label_set_text(status_, !model.error().empty()
                                   ? "Playback unavailable"
                                   : (model.loading() ? "Loading…" : (model.playing() ? "Playing" : "Ready to play")));
    for (auto *control : {play_, favorite_, progress_}) {
        if (track)
            lv_obj_remove_state(control, LV_STATE_DISABLED);
        else
            lv_obj_add_state(control, LV_STATE_DISABLED);
    }
    const bool had_error = !lv_obj_has_flag(error_, LV_OBJ_FLAG_HIDDEN);
    if (model.error().empty())
        lv_obj_add_flag(error_, LV_OBJ_FLAG_HIDDEN);
    else {
        lv_label_set_text(error_, model.error().c_str());
        lv_obj_remove_flag(error_, LV_OBJ_FLAG_HIDDEN);
    }
    if (had_error != !model.error().empty()) apply_layout();
    update_library();
}
