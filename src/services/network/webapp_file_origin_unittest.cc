// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/webapp_file_origin.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/referrer_policy.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
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
const char kTileUrl[] = "https://tile.openstreetmap.org/15/17602/10746.png";

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

class WebAppReferrerForRequestTest : public testing::Test {
 protected:
  void SetUp() override {
    context_ = net::CreateTestURLRequestContextBuilder()->Build();
  }

  std::unique_ptr<net::URLRequest> CreateRequest(const GURL& url) {
    return context_->CreateRequest(url, net::DEFAULT_PRIORITY, &delegate_,
                                   TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // A request from the Maps application, as the network service sees it
  // once ConfigureUrlRequest() took the initiator from the factory lock.
  std::unique_ptr<net::URLRequest> CreateAppRequest(const GURL& url) {
    std::unique_ptr<net::URLRequest> request = CreateRequest(url);
    request->set_initiator(AppOrigin(kMapsAppId));
    return request;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<net::URLRequestContext> context_;
  net::TestDelegate delegate_;
  net::HttpRequestHeaders headers_;
};

TEST_F(WebAppReferrerForRequestTest, ApplicationRequestIsIdentified) {
  EXPECT_EQ(GURL(kMapsAppUrl), WebAppReferrerForRequest(
                                   *CreateAppRequest(GURL(kTileUrl)), headers_));
  EXPECT_EQ(GURL(kMapsAppUrl),
            WebAppReferrerForRequest(
                *CreateAppRequest(GURL("http://example.com/")), headers_));
}

TEST_F(WebAppReferrerForRequestTest, ExistingReferrerIsKept) {
  std::unique_ptr<net::URLRequest> request = CreateAppRequest(GURL(kTileUrl));
  request->SetReferrer("https://example.com/page");
  EXPECT_TRUE(WebAppReferrerForRequest(*request, headers_).is_empty());
}

TEST_F(WebAppReferrerForRequestTest, ExistingRefererHeaderIsKept) {
  headers_.SetHeader(net::HttpRequestHeaders::kReferer,
                     "https://example.com/page");
  EXPECT_TRUE(
      WebAppReferrerForRequest(*CreateAppRequest(GURL(kTileUrl)), headers_)
          .is_empty());
}

TEST_F(WebAppReferrerForRequestTest, NoReferrerPolicyIsObeyed) {
  std::unique_ptr<net::URLRequest> request = CreateAppRequest(GURL(kTileUrl));
  request->set_referrer_policy(net::ReferrerPolicy::NO_REFERRER);
  EXPECT_TRUE(WebAppReferrerForRequest(*request, headers_).is_empty());
}

TEST_F(WebAppReferrerForRequestTest, OnlyHttpRequestsGetOne) {
  EXPECT_TRUE(WebAppReferrerForRequest(
                  *CreateAppRequest(GURL("wss://example.com/socket")),
                  headers_)
                  .is_empty());
  EXPECT_TRUE(WebAppReferrerForRequest(
                  *CreateAppRequest(GURL("file:///usr/palm/x.png")), headers_)
                  .is_empty());
}

TEST_F(WebAppReferrerForRequestTest, RequestsNotFromAnApplicationGetNone) {
  std::unique_ptr<net::URLRequest> no_initiator = CreateRequest(GURL(kTileUrl));
  EXPECT_TRUE(WebAppReferrerForRequest(*no_initiator, headers_).is_empty());

  std::unique_ptr<net::URLRequest> plain_file = CreateRequest(GURL(kTileUrl));
  plain_file->set_initiator(url::Origin::Create(GURL(kMapsDocument)));
  EXPECT_TRUE(WebAppReferrerForRequest(*plain_file, headers_).is_empty());

  std::unique_ptr<net::URLRequest> web_page = CreateRequest(GURL(kTileUrl));
  web_page->set_initiator(url::Origin::Create(GURL("https://example.com/")));
  EXPECT_TRUE(WebAppReferrerForRequest(*web_page, headers_).is_empty());
}

}  // namespace
#endif  // defined(USE_NEVA_APPRUNTIME)

}  // namespace network
