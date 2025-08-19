// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_UTILS_H_
#define IPC_IPC_MESSAGE_UTILS_H_

// NEVA: M151 split this header - the serialization half survives as
// ipc/param_traits_utils.h, the legacy message plumbing was deleted. Forward
// to the survivor so LG's //ozone layer keeps compiling; anything genuinely
// missing is added below rather than by re-vendoring the whole 1167-line
// original, which would duplicate every ParamTraits specialization.
#include "ipc/ipc_message.h"
#include "ipc/param_traits.h"
#include "ipc/param_traits_utils.h"

#endif  // IPC_IPC_MESSAGE_UTILS_H_
