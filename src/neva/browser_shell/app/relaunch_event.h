// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_BROWSER_SHELL_APP_RELAUNCH_EVENT_H_
#define NEVA_BROWSER_SHELL_APP_RELAUNCH_EVENT_H_

#include <string_view>

#include "base/values.h"

namespace browser_shell {

// The detail of the "webOSRelaunch" event that a relaunch fires in the app's
// page: an intent's action and uri, or the URL to open ("url", from the
// launch's "target"), and the system's command ("palm-command"), such as
// "open-app-menu" when the status bar's app menu is tapped. Empty when the
// relaunch carries none of these, and then no event is fired.
base::DictValue RelaunchEventDetail(std::string_view reason,
                                    const base::DictValue& params);

}  // namespace browser_shell

#endif  // NEVA_BROWSER_SHELL_APP_RELAUNCH_EVENT_H_
