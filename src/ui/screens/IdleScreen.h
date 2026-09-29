#pragma once

#include "ui/Screen.h"

namespace ui {

// Idle (PLAN.md §9.3): speaker chip, menu button, "Tap a card to play".
// Phase 1 placeholder: the Resume row and the offline banner come in Phase 4.
class IdleScreen : public Screen {
 public:
  void build(lv_obj_t *root, const Model &model) override;
  void refresh(const Model &model) override;

 private:
  lv_obj_t *speakerLabel_ = nullptr;
  lv_obj_t *statusLabel_ = nullptr;
};

}  // namespace ui
