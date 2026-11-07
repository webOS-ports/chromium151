// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_PAL_SERVICE_WEBOS_NETWORK_ERROR_SETTINGS_APP_H_
#define NEVA_PAL_SERVICE_WEBOS_NETWORK_ERROR_SETTINGS_APP_H_

#include <string_view>

namespace pal {
namespace webos {

// The settings the network error page asks for, as it passes them to
// errorPageController.settingsButtonClick() (TargetEnum in
// webos/browser/resources/webos_network_error_page_template.js).
enum NetworkErrorSettingsTarget {
  kNetworkErrorSettingsNetwork = 1,
  kNetworkErrorSettingsGeneral = 2,
};

// The app that shows those settings. LuneOS has no single settings app taking
// a target, like LG's com.palm.app.settings: each settings page is its own QML
// app, org.webosports.app.settings.<page>, all running the settings app's
// GenericCategoryWindow.qml. Empty for a target the page does not send.
constexpr std::string_view SettingsAppIdForTarget(int target_id) {
  switch (target_id) {
    case kNetworkErrorSettingsNetwork:
      // The page asks for this on "check network connection" errors (no
      // network, name not resolved). Wi-Fi is where that is fixed; the
      // "networksettings" page is the mobile network (SIM, APN, operator).
      return "org.webosports.app.settings.wifi";
    case kNetworkErrorSettingsGeneral:
      // Only asked for on CERT_DATE_INVALID: the device clock is wrong.
      return "org.webosports.app.settings.dateandtime";
  }
  return std::string_view();
}

}  // namespace webos
}  // namespace pal

#endif  // NEVA_PAL_SERVICE_WEBOS_NETWORK_ERROR_SETTINGS_APP_H_
