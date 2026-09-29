#pragma once

#include "ui/Screen.h"

namespace ui {

// First frame at power-on, shown while the tasks start.
class BootScreen : public Screen {
 public:
  void build(lv_obj_t *root, const Model &model) override;
};

}  // namespace ui
