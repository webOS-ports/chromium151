// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEBAPP_FILE_ORIGIN_H_
#define SERVICES_NETWORK_WEBAPP_FILE_ORIGIN_H_

#include "base/component_export.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
class URLRequest;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace network {

#if defined(USE_NEVA_APPRUNTIME)
// webOS applications are file: pages whose origin carries the application
// id, set by the browser. Returns the URL that identifies such an
// application, file://<app id>-webos/ - the host location.origin reports -
// or an empty GURL for any other origin.
COMPONENT_EXPORT(NETWORK_SERVICE)
GURL WebAppFileOriginUrl(const url::Origin& origin);

// Chromium sends no Referer from file:, so a request from a webOS
// application cannot be told apart from any other. Returns the Referer an
// http(s) request from an application gets when it has none, the
// application's WebAppFileOriginUrl(); an empty GURL when the request keeps
// what it has (it has a Referer, asked for none, or is not from an
// application).
COMPONENT_EXPORT(NETWORK_SERVICE)
GURL WebAppReferrerForRequest(const net::URLRequest& request,
                              const net::HttpRequestHeaders& headers);
#endif  // defined(USE_NEVA_APPRUNTIME)

}  // namespace network

#endif  // SERVICES_NETWORK_WEBAPP_FILE_ORIGIN_H_
