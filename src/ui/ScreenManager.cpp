#include "ui/ScreenManager.h"

#include <memory>

#include "ui/LvglPort.h"
#include "ui/Theme.h"
#include "ui/screens/BootScreen.h"
#include "ui/screens/ConnectingScreen.h"
#include "ui/screens/IdleScreen.h"
#include "ui/screens/MenuScreen.h"
#include "ui/screens/PlaceholderScreen.h"
#include "ui/screens/SetupScreen.h"
#include "ui/screens/WifiScreen.h"
#include "util/Log.h"

namespace ui::screens {

namespace {

constexpr const char *kTag = "ui";

Model current;
std::unique_ptr<Screen> active;
app::ScreenId activeId = app::ScreenId::Boot;
bool idleLogged = false;

std::unique_ptr<Screen> create(app::ScreenId id) {
  switch (id) {
    case app::ScreenId::Boot:
      return std::make_unique<BootScreen>();
    case app::ScreenId::Idle:
      return std::make_unique<IdleScreen>();
    case app::ScreenId::Menu:
      return std::make_unique<MenuScreen>();
    case app::ScreenId::Setup:
      return std::make_unique<SetupScreen>();
    case app::ScreenId::Connecting:
      return std::make_unique<ConnectingScreen>();
    case app::ScreenId::Wifi:
      return std::make_unique<WifiScreen>();
    default:
      return std::make_unique<PlaceholderScreen>(id);
  }
}

}  // namespace

void apply(const Model &model) {
  current = model;
  const app::ScreenId id = app::screenFor(model.state);
  if (active && id == activeId) {
    active->refresh(model);
    return;
  }

  std::unique_ptr<Screen> next = create(id);
  lv_obj_t *root = theme::createScreen();
  next->build(root, model);
  lv_obj_t *old = lv_screen_active();
  lv_screen_load(root);
  if (old) lv_obj_delete(old);  // Also removes the old screen's event callbacks
  active = std::move(next);
  activeId = id;
  LOG_D(kTag, "Screen: %s", app::name(id));

  if (id == app::ScreenId::Idle && !idleLogged) {
    idleLogged = true;
    port::renderNow();
    LOG_I(kTag, "Idle on screen %lu ms after start", millis());
  }
}

const Model &model() { return current; }

}  // namespace ui::screens
