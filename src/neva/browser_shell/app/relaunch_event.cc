// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/browser_shell/app/relaunch_event.h"

#include <string>

namespace browser_shell {

namespace {

const char kAction[] = "action";
const char kIntentService[] = "com.webos.service.intent";
const char kPalmCommand[] = "palm-command";
const char kTarget[] = "target";
const char kUri[] = "uri";

}  // namespace

base::DictValue RelaunchEventDetail(std::string_view reason,
                                    const base::DictValue& params) {
  base::DictValue detail;
  if (reason == kIntentService) {
    const std::string* action = params.FindString(kAction);
    const std::string* uri = params.FindString(kUri);
    if (action && !action->empty() && uri && !uri->empty()) {
      detail.Set("action", *action);
      detail.Set("uri", *uri);
    }
  } else {
    const std::string* target_url = params.FindString(kTarget);
    if (target_url && !target_url->empty()) {
      detail.Set("url", *target_url);
    }
  }

  // The system's own commands to the app, whatever the reason given. webOS
  // relaunches an app with {"palm-command": "open-app-menu"} when its menu
  // is tapped in the status bar, and the app answers by showing its menu.
  const std::string* command = params.FindString(kPalmCommand);
  if (command && !command->empty()) {
    detail.Set(kPalmCommand, *command);
  }
  return detail;
}

}  // namespace browser_shell
