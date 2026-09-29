#pragma once

#include "ui/Model.h"

// Shows the screen that app::screenFor(model.state) names (UI task only).
// Screens are rebuilt on every switch, which keeps only one screen's widgets in
// memory; a model change that keeps the same screen just refreshes it.
namespace ui::screens {

void apply(const Model &model);
const Model &model();

}  // namespace ui::screens
