// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/pal_service/webos/network_error_settings_app.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace pal {
namespace webos {
namespace {

// The ids the page sends: TargetEnum in
// webos/browser/resources/webos_network_error_page_template.js.
constexpr int kPageNetwork = 1;
constexpr int kPageGeneral = 2;

TEST(NetworkErrorSettingsAppTest, NetworkOpensWiFi) {
  EXPECT_EQ(SettingsAppIdForTarget(kPageNetwork),
            "org.webosports.app.settings.wifi");
}

TEST(NetworkErrorSettingsAppTest, GeneralOpensDateAndTime) {
  // Only CERT_DATE_INVALID asks for it: the clock is what needs setting.
  EXPECT_EQ(SettingsAppIdForTarget(kPageGeneral),
            "org.webosports.app.settings.dateandtime");
}

TEST(NetworkErrorSettingsAppTest, UnknownTargetLaunchesNothing) {
  EXPECT_TRUE(SettingsAppIdForTarget(0).empty());
  EXPECT_TRUE(SettingsAppIdForTarget(3).empty());
  EXPECT_TRUE(SettingsAppIdForTarget(-1).empty());
}

TEST(NetworkErrorSettingsAppTest, NoLgSettingsApp) {
  // LuneOS has no com.palm.app.settings; launching it fails silently.
  for (const int target : {kPageNetwork, kPageGeneral}) {
    EXPECT_EQ(SettingsAppIdForTarget(target).find("com.palm."),
              std::string_view::npos);
  }
}

// The enum has to keep the page's numbers.
static_assert(kNetworkErrorSettingsNetwork == kPageNetwork);
static_assert(kNetworkErrorSettingsGeneral == kPageGeneral);

// Usable in constant expressions, so it costs nothing at run time.
static_assert(!SettingsAppIdForTarget(kPageNetwork).empty());
static_assert(SettingsAppIdForTarget(99).empty());

}  // namespace
}  // namespace webos
}  // namespace pal
