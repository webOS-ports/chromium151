// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_SHELL_BROWSER_SHELL_DISPLAY_INFO_PROVIDER_H_
#define NEVA_APP_SHELL_BROWSER_SHELL_DISPLAY_INFO_PROVIDER_H_

#include "extensions/browser/api/system_display/display_info_provider.h"

namespace extensions {

class ShellDisplayInfoProvider : public DisplayInfoProvider {
 public:
  ShellDisplayInfoProvider();

  ShellDisplayInfoProvider(const ShellDisplayInfoProvider&) = delete;
  ShellDisplayInfoProvider& operator=(const ShellDisplayInfoProvider&) = delete;
};

}  // namespace extensions

#endif  // NEVA_APP_SHELL_BROWSER_SHELL_DISPLAY_INFO_PROVIDER_H_
