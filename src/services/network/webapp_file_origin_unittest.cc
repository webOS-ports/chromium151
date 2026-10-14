// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/webapp_file_origin.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

#if defined(USE_NEVA_APPRUNTIME)
namespace {

const char kMapsDocument[] =
    "file:///usr/palm/applications/org.webosports.app.maps/index.html";
const char kMapsAppId[] = "org.webosports.app.maps";
const char kMapsAppUrl[] = "file://org.webosports.app.maps-webos/";

// The origin the browser gives a frame of an application, see
// RenderFrameHostImpl::SetLastCommittedOrigin().
url::Origin AppOrigin(const std::string& app_id) {
  url::Origin origin = url::Origin::Create(GURL(kMapsDocument));
  origin.set_webapp_id(app_id);
  return origin;
}

TEST(WebAppFileOriginUrlTest, ApplicationGetsItsOwnHost) {
  EXPECT_EQ(GURL(kMapsAppUrl), WebAppFileOriginUrl(AppOrigin(kMapsAppId)));
}

TEST(WebAppFileOriginUrlTest, HostIsLowerCase) {
  // location.origin reports the host in lower case as well.
  EXPECT_EQ(GURL("file://com.palm.app.calendar-webos/"),
            WebAppFileOriginUrl(AppOrigin("com.palm.App.Calendar")));
}

TEST(WebAppFileOriginUrlTest, ApplicationsAreToldApart) {
  EXPECT_NE(WebAppFileOriginUrl(AppOrigin(kMapsAppId)),
            WebAppFileOriginUrl(AppOrigin("com.palm.app.calendar")));
}

TEST(WebAppFileOriginUrlTest, FileOriginWithoutApplicationHasNone) {
  EXPECT_TRUE(
      WebAppFileOriginUrl(url::Origin::Create(GURL(kMapsDocument))).is_empty());
  EXPECT_TRUE(WebAppFileOriginUrl(AppOrigin("")).is_empty());
}

TEST(WebAppFileOriginUrlTest, OnlyFileOriginsAreApplications) {
  url::Origin web_origin = url::Origin::Create(GURL("https://example.com/"));
  web_origin.set_webapp_id(kMapsAppId);
  EXPECT_TRUE(WebAppFileOriginUrl(web_origin).is_empty());
}

TEST(WebAppFileOriginUrlTest, IdThatIsNoValidHostHasNone) {
  EXPECT_TRUE(WebAppFileOriginUrl(AppOrigin("a b/c")).is_empty());
}

}  // namespace
#endif  // defined(USE_NEVA_APPRUNTIME)

}  // namespace network
