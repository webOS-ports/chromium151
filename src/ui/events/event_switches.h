// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_SWITCHES_H_
#define UI_EVENTS_EVENT_SWITCHES_H_

#include "build/build_config.h"
#include "ui/events/events_base_export.h"

namespace switches {

EVENTS_BASE_EXPORT extern const char kCompensateForUnstablePinchZoom[];
EVENTS_BASE_EXPORT extern const char kTouchSlopDistance[];

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
EVENTS_BASE_EXPORT extern const char kTouchDevices[];
EVENTS_BASE_EXPORT extern const char kPenDevices[];
#endif

#if BUILDFLAG(IS_OZONE)
EVENTS_BASE_EXPORT extern const char kEdgeTouchFiltering[];
EVENTS_BASE_EXPORT extern const char kDisableCancelAllTouches[];
EVENTS_BASE_EXPORT
extern const char kEnableMicrophoneMuteSwitchDeviceSwitch[];

#endif

// NEVA: these were guarded on OZONE_PLATFORM_WAYLAND_EXTERNAL, LG's legacy
// ozone backend. WaylandSeatManager in the modern wayland path uses them too,
// so they are keyed off the neva app runtime instead.
#if defined(USE_OZONE) && defined(USE_NEVA_APPRUNTIME)
EVENTS_BASE_EXPORT extern const char kIgnoreTouchDevices[];
EVENTS_BASE_EXPORT extern const char kForceMaxTouchPoints[];
#endif  // defined(USE_OZONE) && defined(USE_NEVA_APPRUNTIME)

}  // namespace switches

#endif  // UI_EVENTS_EVENT_SWITCHES_H_
