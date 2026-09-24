#pragma once

#include "pages/music/music_view_model.hpp"
#include <lvgl.h>
#include <string>
#include <vector>

class MusicView {
public:
    explicit MusicView(MusicViewModel &view_model) noexcept : view_model_(view_model)
    {
    }
    ~MusicView()
    {
        destroy();
    }
    lv_obj_t *create(lv_obj_t *parent, const char *application_name);
    void destroy() noexcept;

private:
    enum class Action { play, next, previous, shuffle, repeat, favorite, all, favorites, library, player, rescan };
    struct Command {
        MusicView *view;
        Action action;
        std::size_t index = 0;
        bool track        = false;
    };
    static void command_event(lv_event_t *event);
    static void size_event(lv_event_t *event);
    static void state_changed(lv_observer_t *observer, lv_subject_t *subject);
    static void slider_event(lv_event_t *event);
    static void search_event(lv_event_t *event);
    static void keyboard_event(lv_event_t *event);
    static void art_draw(lv_event_t *event);
    lv_obj_t *button(lv_obj_t *parent, const char *text, Action action, const char *name, int width = 48);
    lv_obj_t *artwork(lv_obj_t *parent, int size, unsigned theme);
    void apply_layout();
    void update();
    void update_library();
    void show_library(bool show);
    MusicViewModel &view_model_;
    lv_obj_t *surface_          = nullptr;
    lv_obj_t *header_           = nullptr;
    lv_obj_t *tabs_             = nullptr;
    lv_obj_t *library_tab_      = nullptr;
    lv_obj_t *player_tab_       = nullptr;
    lv_obj_t *body_             = nullptr;
    lv_obj_t *library_          = nullptr;
    lv_obj_t *library_count_    = nullptr;
    lv_obj_t *all_button_       = nullptr;
    lv_obj_t *favorites_button_ = nullptr;
    lv_obj_t *search_           = nullptr;
    lv_obj_t *list_             = nullptr;
    lv_obj_t *library_footer_   = nullptr;
    lv_obj_t *player_           = nullptr;
    lv_obj_t *player_main_      = nullptr;
    lv_obj_t *art_area_         = nullptr;
    lv_obj_t *cover_            = nullptr;
    lv_obj_t *source_           = nullptr;
    lv_obj_t *controls_         = nullptr;
    lv_obj_t *song_title_       = nullptr;
    lv_obj_t *artist_           = nullptr;
    lv_obj_t *album_            = nullptr;
    lv_obj_t *favorite_         = nullptr;
    lv_obj_t *progress_         = nullptr;
    lv_obj_t *elapsed_          = nullptr;
    lv_obj_t *remaining_        = nullptr;
    lv_obj_t *play_             = nullptr;
    lv_obj_t *shuffle_          = nullptr;
    lv_obj_t *repeat_           = nullptr;
    lv_obj_t *volume_           = nullptr;
    lv_obj_t *status_           = nullptr;
    lv_obj_t *error_            = nullptr;
    lv_obj_t *keyboard_         = nullptr;
    lv_timer_t *timer_          = nullptr;
    std::vector<std::unique_ptr<Command>> commands_;
    std::vector<std::unique_ptr<Command>> row_commands_;
    std::string library_key_;
    bool library_visible_ = false;
    bool wide_            = false;
    bool layout_busy_     = false;
};
