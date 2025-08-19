// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_GPU_VSYNC_CALLBACK_H_
#define COMPONENTS_VIZ_COMMON_GPU_GPU_VSYNC_CALLBACK_H_

#include "base/functional/callback.h"
#include "base/time/time.h"

// NEVA: M151 deleted this header along with the rest of upstream's GpuVSync
// plumbing. The neva ozone/wayland GL surface still drives its own vsync timer
// through a callback of this shape (GLSurfaceWayland::SetVSyncCallback), so the
// type alias is kept here, unchanged, rather than rewriting those call sites.

namespace viz {

using GpuVSyncCallback =
    base::RepeatingCallback<void(base::TimeTicks vsync_time,
                                 base::TimeDelta vsync_interval)>;

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_GPU_VSYNC_CALLBACK_H_
