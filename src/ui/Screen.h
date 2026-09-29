#pragma once

#include <lvgl.h>

#include "ui/Model.h"

namespace ui {

// One full screen. ScreenManager creates it when the app state calls for it and
// deletes it (with its widgets) when another screen replaces it.
class Screen {
 public:
  virtual ~Screen() = default;

  // Create the widgets as children of `root`.
  virtual void build(lv_obj_t *root, const Model &model) = 0;

  // The model changed while this screen is showing.
  virtual void refresh(const Model &model) { (void)model; }
};

}  // namespace ui
