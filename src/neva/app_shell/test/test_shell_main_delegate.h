// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_SHELL_TEST_TEST_SHELL_MAIN_DELEGATE_H_
#define NEVA_APP_SHELL_TEST_TEST_SHELL_MAIN_DELEGATE_H_

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "neva/app_shell/app/shell_main_delegate.h"
#include <optional>


namespace content {
class ContentUtilityClient;
}

namespace extensions {

class TestShellMainDelegate : public extensions::ShellMainDelegate {
 public:
  TestShellMainDelegate();

  TestShellMainDelegate(const TestShellMainDelegate&) = delete;
  TestShellMainDelegate& operator=(const TestShellMainDelegate&) = delete;

  ~TestShellMainDelegate() override;

  // ContentMainDelegate implementation:

 protected:
  // content::ContentMainDelegate implementation:
  content::ContentUtilityClient* CreateContentUtilityClient() override;

 private:
  std::unique_ptr<content::ContentUtilityClient> utility_client_;

};

}  // namespace extensions

#endif  // NEVA_APP_SHELL_TEST_TEST_SHELL_MAIN_DELEGATE_H_
