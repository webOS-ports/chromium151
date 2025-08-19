// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NEVA: the two helpers below are declared in web_app_utils.h but are the only
// things the enable_pwa_manager_webapi "components_minimal" target needs from
// it. web_app_utils.cc itself is Profile-heavy (AreWebAppsEnabled,
// ConstructWebAppErrorPage, ...) and pulling it into components_minimal would
// drag //chrome/browser/profiles - and with it a second copy of
// //components/webapps/browser - into libcbe.so. They live here so both the
// full //chrome/browser/web_applications target and components_minimal can
// compile them without duplicating symbols.

#include "chrome/browser/web_applications/web_app_utils.h"

#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace web_app {

bool IsInScope(const GURL& url, const GURL& scope) {
  if (!scope.is_valid()) {
    return false;
  }

  return base::StartsWith(url.spec(), scope.spec(),
                          base::CompareCase::SENSITIVE);
}

const char* IconsDownloadedResultToString(IconsDownloadedResult result) {
  switch (result) {
    case IconsDownloadedResult::kCompleted:
      return "Completed";
    case IconsDownloadedResult::kPrimaryPageChanged:
      return "PrimaryPageChanged";
    case IconsDownloadedResult::kAbortedDueToFailure:
      return "AbortedDueToFailure";
  }
}

}  // namespace web_app
