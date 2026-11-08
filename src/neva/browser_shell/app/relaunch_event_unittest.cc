// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/browser_shell/app/relaunch_event.h"

#include "base/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_shell {

namespace {

base::DictValue Detail(std::string_view reason, std::string_view params) {
  return RelaunchEventDetail(reason, base::test::ParseJsonDict(params));
}

}  // namespace

TEST(RelaunchEventTest, TargetBecomesUrl) {
  EXPECT_EQ(Detail("", R"({"target": "https://example.com/"})"),
            base::test::ParseJsonDict(R"({"url": "https://example.com/"})"));
}

TEST(RelaunchEventTest, IntentGivesActionAndUri) {
  EXPECT_EQ(Detail("com.webos.service.intent",
                   R"({"action": "view", "uri": "https://example.com/",
                       "target": "ignored"})"),
            base::test::ParseJsonDict(
                R"({"action": "view", "uri": "https://example.com/"})"));
}

// The status bar's app menu: the command has to reach the page, or the app
// never hears that its menu was asked for.
TEST(RelaunchEventTest, PalmCommandIsPassedOn) {
  EXPECT_EQ(Detail("", R"({"palm-command": "open-app-menu"})"),
            base::test::ParseJsonDict(R"({"palm-command": "open-app-menu"})"));
}

TEST(RelaunchEventTest, PalmCommandWithATarget) {
  EXPECT_EQ(Detail("", R"({"palm-command": "open-app-menu",
                           "target": "https://example.com/"})"),
            base::test::ParseJsonDict(R"({"palm-command": "open-app-menu",
                                          "url": "https://example.com/"})"));
}

TEST(RelaunchEventTest, NothingToTellIsEmpty) {
  EXPECT_TRUE(Detail("", R"({})").empty());
  EXPECT_TRUE(Detail("", R"({"target": "", "palm-command": ""})").empty());
  EXPECT_TRUE(Detail("", R"({"target": 3, "palm-command": true})").empty());
  EXPECT_TRUE(
      Detail("com.webos.service.intent", R"({"action": "view"})").empty());
}

}  // namespace browser_shell
