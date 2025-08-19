// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_SHELL_BROWSER_SHELL_DISPLAY_INFO_PROVIDER_H_
#define NEVA_APP_SHELL_BROWSER_SHELL_DISPLAY_INFO_PROVIDER_H_

#include "extensions/browser/api/system_display/display_info_provider.h"
#include "ui/display/screen.h"

namespace extensions {

class ShellDisplayInfoProvider : public DisplayInfoProvider {
 public:
  explicit ShellDisplayInfoProvider(display::Screen* screen);

  ShellDisplayInfoProvider(const ShellDisplayInfoProvider&) = delete;

 protected:
  // M151: app_shell has no system.display event router to notify.
  void DispatchOnDisplayChangedEvent() override {}

 public:
  ShellDisplayInfoProvider& operator=(const ShellDisplayInfoProvider&) = delete;
};

}  // namespace extensions

#endif  // NEVA_APP_SHELL_BROWSER_SHELL_DISPLAY_INFO_PROVIDER_H_
