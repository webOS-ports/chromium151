// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_shell/browser/shell_display_info_provider.h"

namespace extensions {

ShellDisplayInfoProvider::ShellDisplayInfoProvider(display::Screen* screen)
    : DisplayInfoProvider(screen) {}

}  // namespace extensions
