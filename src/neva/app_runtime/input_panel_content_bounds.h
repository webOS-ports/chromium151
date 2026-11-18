// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_RUNTIME_INPUT_PANEL_CONTENT_BOUNDS_H_
#define NEVA_APP_RUNTIME_INPUT_PANEL_CONTENT_BOUNDS_H_

#include "ui/gfx/geometry/rect.h"

namespace neva_app_runtime {

struct InputPanelContentBounds {
  gfx::Rect bounds;
  bool resized_for_panel = false;
};

// Returns the bounds for a web app's contents given the input panel's state.
// `full_height` is the contents' height with no panel, for the window's current
// size and orientation. `panel_top` is the panel's top edge, in the same units
// as `bounds`, and is used only when `panel_wanted` is true.
InputPanelContentBounds ComputeInputPanelContentBounds(const gfx::Rect& bounds,
                                                       int full_height,
                                                       bool panel_wanted,
                                                       int panel_top,
                                                       bool resized_for_panel);

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_INPUT_PANEL_CONTENT_BOUNDS_H_
