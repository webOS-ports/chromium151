// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/webapp_file_origin.h"

#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace network {

#if defined(USE_NEVA_APPRUNTIME)
GURL WebAppFileOriginUrl(const url::Origin& origin) {
  const std::optional<std::string> app_id = origin.get_webapp_id();
  if (origin.scheme() != url::kFileScheme || !app_id || app_id->empty()) {
    return GURL();
  }
  // Same host as the application's security origin, see
  // neva_app_runtime::CreateFileSecurityOriginForApp() and WAM's
  // IdentifierForSecurityOrigin().
  const std::string host = base::StrCat({base::ToLowerASCII(*app_id), "-webos"});
  GURL app_url(base::StrCat(
      {url::kFileScheme, url::kStandardSchemeSeparator, host, "/"}));
  // An id that is not a plain host name (e.g. one with a '/') would leave
  // only part of it as the host, and two applications could share it.
  if (!app_url.is_valid() || app_url.host() != host) {
    return GURL();
  }
  return app_url;
}
#endif  // defined(USE_NEVA_APPRUNTIME)

}  // namespace network
