#pragma once

#include "ui/Screen.h"

namespace ui {

// Stands in for screens that later phases build, saying which phase brings it.
// Overlays get a back button; the rest show the status line.
class PlaceholderScreen : public Screen {
 public:
  explicit PlaceholderScreen(app::ScreenId id) : id_(id) {}
  void build(lv_obj_t *root, const Model &model) override;
  void refresh(const Model &model) override;

 private:
  app::ScreenId id_;
  lv_obj_t *statusLabel_ = nullptr;
};

}  // namespace ui
