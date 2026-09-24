#include "app.hpp"
#include "app_identity.h"
#include "pages/music/music_view.hpp"
#include <memory>

namespace {
struct AppState {
    std::unique_ptr<MusicViewModel> view_model;
    std::unique_ptr<MusicView> view;
};
AppState app_state;
void app_open(const cm0_app_context_t *context)
{
    app_state.view_model = std::make_unique<MusicViewModel>();
    app_state.view       = std::make_unique<MusicView>(*app_state.view_model);
    app_state.view->create(context->root, LILYGO_UI_MUSIC_APP_NAME);
}
void app_close()
{
    app_state.view.reset();
    app_state.view_model.reset();
}
const cm0_app_descriptor_t app_descriptor{
    CM0_APP_API_VERSION, sizeof(cm0_app_descriptor_t), LILYGO_UI_MUSIC_APP_ID, app_open, app_close,
};
}  // namespace
extern "C" const cm0_app_descriptor_t *cm0_app_get_descriptor()
{
    return &app_descriptor;
}
