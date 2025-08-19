// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from chrome/browser/push_messaging/push_messaging_features.cc

#include "neva/app_runtime/browser/push_messaging/push_messaging_features.h"

namespace features {

BASE_FEATURE(kPushMessagingDisallowSenderIDs,
             "PushMessagingDisallowSenderIDs",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPushSubscriptionWithExpirationTime,
             "PushSubscriptionWithExpirationTime",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
