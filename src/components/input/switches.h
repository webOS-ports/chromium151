// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_SWITCHES_H_
#define COMPONENTS_INPUT_SWITCHES_H_

#include "base/component_export.h"

namespace input::switches {

COMPONENT_EXPORT(INPUT) extern const char kDisableHangMonitor[];
COMPONENT_EXPORT(INPUT) extern const char kDisablePinch[];
// NEVA: throttle key events so only one is in flight to the renderer.
COMPONENT_EXPORT(INPUT) extern const char kEnableKeyEventThrottling[];
COMPONENT_EXPORT(INPUT) extern const char kValidateInputEventStream[];
COMPONENT_EXPORT(INPUT) bool IsPinchToZoomEnabled();

}  // namespace input::switches

#endif  // COMPONENTS_INPUT_SWITCHES_H_
