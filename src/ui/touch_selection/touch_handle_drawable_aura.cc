// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/touch_handle_drawable_aura.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/touch_selection//vector_icons/vector_icons.h"

namespace ui {
namespace {

// Padding to apply horizontally around and vertically below the handle image,
// so that touch events near the handle image are targeted to the handle.
constexpr int kSelectionHandlePadding = 6;

// Max opacity of the selection handle image.
constexpr float kSelectionHandleMaxOpacity = 0.8f;

// Epsilon value used to compare float values to zero.
constexpr float kEpsilon = 1e-8f;

// Returns the appropriate handle vector icon based on the handle orientation.
ImageModel GetHandleVectorIcon(TouchHandleOrientation orientation) {
  const gfx::VectorIcon* icon = nullptr;
  switch (orientation) {
    case TouchHandleOrientation::LEFT:
      icon = &kTextSelectionHandleLeftCustomIcon;
      break;
    case TouchHandleOrientation::CENTER:
      icon = &kTextSelectionHandleCenterCustomIcon;
      break;
    case TouchHandleOrientation::RIGHT:
      icon = &kTextSelectionHandleRightCustomIcon;
      break;
    case TouchHandleOrientation::UNDEFINED:
      NOTREACHED() << "Invalid touch handle bound type.";
  }
  // webOS drew its selection markers in a near-white grey - the legacy
  // topmarker.png/bottommarker.png the browser carried are #F2F2F2 shading to
  // #C8C8C8 - and nothing on this device is the theme's primary blue, which is
  // what these handles are otherwise painted in.
  return ImageModel::FromVectorIcon(*icon,
                                    /*color=*/SkColorSetRGB(0xF2, 0xF2, 0xF2));
}

bool IsNearlyZero(float value) {
  return std::abs(value) < kEpsilon;
}

}  // namespace

TouchHandleDrawableAura::TouchHandleDrawableAura(aura::Window* parent)
    : window_(std::make_unique<aura::Window>(/*delegate=*/nullptr)),
      enabled_(false),
      alpha_(0),
      orientation_(TouchHandleOrientation::UNDEFINED) {
  window_->Init(LAYER_TEXTURED);
  window_->SetTransparent(true);
  window_->set_owned_by_parent(false);
  window_->SetEventTargetingPolicy(aura::EventTargetingPolicy::kNone);
  window_->layer()->set_delegate(this);
  parent->AddChild(window_.get());

  theme_observation_.Observe(NativeTheme::GetInstanceForNativeUi());
}

TouchHandleDrawableAura::~TouchHandleDrawableAura() = default;

void TouchHandleDrawableAura::UpdateWindowBounds() {
  gfx::Rect window_bounds(gfx::ToRoundedPoint(targetable_origin_),
                          handle_image_.Size());
  // Offset the window bounds to account for space between the origin of the
  // targetable area and the handle image.
  window_bounds.Offset(kSelectionHandlePadding, 0);
  window_->SetBounds(window_bounds);
}

bool TouchHandleDrawableAura::IsVisible() const {
  return enabled_ && !IsNearlyZero(alpha_);
}

void TouchHandleDrawableAura::SetEnabled(bool enabled) {
  if (enabled == enabled_)
    return;

  enabled_ = enabled;
  if (IsVisible())
    window_->Show();
  else
    window_->Hide();
}

void TouchHandleDrawableAura::SetOrientation(TouchHandleOrientation orientation,
                                             bool mirror_vertical,
                                             bool mirror_horizontal) {
  // Mirroring vertically is how a handle is put above the line it belongs to
  // rather than below it, which is where webOS drew the one at the start of a
  // selection - pointing down at the text. Drawing it is a matter of turning
  // the same image over, so there is no second icon for it.
  //
  // Mirroring horizontally is still not implemented here, and is ignored
  // rather than fatal: nothing asks for it on this platform.
  if (orientation_ == orientation && mirror_vertical_ == mirror_vertical)
    return;
  orientation_ = orientation;
  mirror_vertical_ = mirror_vertical;

  handle_image_ = GetHandleVectorIcon(orientation);
  UpdateWindowBounds();
  window_->SchedulePaintInRect(gfx::Rect(window_->bounds().size()));
}

void TouchHandleDrawableAura::SetOrigin(const gfx::PointF& position) {
  targetable_origin_ = position;
  UpdateWindowBounds();
}

void TouchHandleDrawableAura::SetAlpha(float alpha) {
  if (alpha == alpha_)
    return;

  alpha_ = alpha;
  window_->layer()->SetOpacity(alpha_ * kSelectionHandleMaxOpacity);

  if (IsVisible())
    window_->Show();
  else
    window_->Hide();
}

gfx::RectF TouchHandleDrawableAura::GetVisibleBounds() const {
  // These bounds are used to determine the area that can be used for targeting
  // the handle, so we include the transparent padding added around the handle
  // image even though it technically isn't visible.
  gfx::RectF targetable_bounds(window_->bounds());
  targetable_bounds.Outset(gfx::OutsetsF::TLBR(0, kSelectionHandlePadding,
                                               kSelectionHandlePadding,
                                               kSelectionHandlePadding));
  return targetable_bounds;
}

float TouchHandleDrawableAura::GetDrawableHorizontalPaddingRatio() const {
  // The ratio returned by this function is used to position the touch handle
  // targetable area relative to the focal point (e.g. bottom of text caret).
  // So, even though padding is applied on both the left and right of the handle
  // image, we compute the ratio based on the padding on only one side.
  return kSelectionHandlePadding /
         (window_->bounds().width() + 2.0f * kSelectionHandlePadding);
}

void TouchHandleDrawableAura::OnPaintLayer(const PaintContext& context) {
  PaintRecorder recorder(context, window_->bounds().size());
  if (handle_image_.IsEmpty()) {
    return;
  }

  const gfx::ImageSkia image =
      handle_image_.Rasterize(ColorProviderManager::Get().GetColorProviderFor(
          NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(nullptr)));
  gfx::Canvas* canvas = recorder.canvas();

  if (!mirror_vertical_) {
    canvas->DrawImageInt(image, 0, 0);
    return;
  }

  // Turned over, so the point that marks the text is at the bottom of the
  // image rather than the top.
  canvas->Save();
  canvas->Translate(gfx::Vector2d(0, image.height()));
  canvas->Scale(1, -1);
  canvas->DrawImageInt(image, 0, 0);
  canvas->Restore();
}

void TouchHandleDrawableAura::OnNativeThemeUpdated(
    NativeTheme* observed_theme) {
  window_->SchedulePaintInRect(gfx::Rect(window_->bounds().size()));
}

}  // namespace ui
