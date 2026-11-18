// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_runtime/input_panel_content_bounds.h"

#include <algorithm>

namespace neva_app_runtime {

InputPanelContentBounds ComputeInputPanelContentBounds(const gfx::Rect& bounds,
                                                       int full_height,
                                                       bool panel_wanted,
                                                       int panel_top,
                                                       bool resized_for_panel) {
  if (!panel_wanted) {
    if (!resized_for_panel) {
      return {bounds, false};
    }
    return {gfx::Rect(bounds.x(), bounds.y(), bounds.width(), full_height),
            false};
  }
  const int height = std::clamp(panel_top - bounds.y(), 0, full_height);
  return {gfx::Rect(bounds.x(), bounds.y(), bounds.width(), height), true};
}

}  // namespace neva_app_runtime
