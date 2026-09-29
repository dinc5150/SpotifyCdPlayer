#pragma once

#include "ui/Screen.h"

namespace ui {

// Menu (PLAN.md §9.2): 2x3 tiles, each opening an overlay.
class MenuScreen : public Screen {
 public:
  void build(lv_obj_t *root, const Model &model) override;
};

}  // namespace ui
