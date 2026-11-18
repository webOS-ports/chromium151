// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_runtime/input_panel_content_bounds.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace neva_app_runtime {

TEST(InputPanelContentBoundsTest, NoPanelLeavesUnresizedContentsAlone) {
  const gfx::Rect bounds(0, 0, 1080, 2045);
  const InputPanelContentBounds result =
      ComputeInputPanelContentBounds(bounds, 2045, false, 0, false);
  EXPECT_EQ(bounds.ToString(), result.bounds.ToString());
  EXPECT_FALSE(result.resized_for_panel);
}

TEST(InputPanelContentBoundsTest, PanelShortensContentsToItsTopEdge) {
  const InputPanelContentBounds result = ComputeInputPanelContentBounds(
      gfx::Rect(0, 40, 1080, 2005), 2005, true, 1300, false);
  EXPECT_EQ(gfx::Rect(0, 40, 1080, 1260).ToString(), result.bounds.ToString());
  EXPECT_TRUE(result.resized_for_panel);
}

TEST(InputPanelContentBoundsTest, HeightStaysWithinTheContents) {
  EXPECT_EQ(0, ComputeInputPanelContentBounds(gfx::Rect(0, 40, 800, 600), 600,
                                              true, 10, false)
                   .bounds.height());
  EXPECT_EQ(600, ComputeInputPanelContentBounds(gfx::Rect(0, 40, 800, 600), 600,
                                                true, 5000, false)
                     .bounds.height());
}

// Shown in landscape and turned to portrait with the panel still up: the room
// is worked out from the portrait height, not the landscape one.
TEST(InputPanelContentBoundsTest, RotationWithPanelUpUsesTheNewFullHeight) {
  const InputPanelContentBounds result = ComputeInputPanelContentBounds(
      gfx::Rect(0, 0, 1080, 905), 2045, true, 1400, true);
  EXPECT_EQ(gfx::Rect(0, 0, 1080, 1400).ToString(), result.bounds.ToString());
  EXPECT_TRUE(result.resized_for_panel);
}

// The panel goes away after the rotation: the contents get the whole portrait
// height back.
TEST(InputPanelContentBoundsTest, PanelHiddenAfterRotationRestoresFullHeight) {
  const InputPanelContentBounds result = ComputeInputPanelContentBounds(
      gfx::Rect(0, 0, 1080, 905), 2045, false, 0, true);
  EXPECT_EQ(gfx::Rect(0, 0, 1080, 2045).ToString(), result.bounds.ToString());
  EXPECT_FALSE(result.resized_for_panel);
}

}  // namespace neva_app_runtime
