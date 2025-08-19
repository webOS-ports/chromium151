// Copyright 2023 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "base/functional/callback.h"
#include "neva/app_runtime/browser/installable/neva_webapps_client.h"

#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace neva_app_runtime {

// static
void NevaWebappsClient::Create() {
  static base::NoDestructor<NevaWebappsClient> instance;
}

security_state::SecurityLevel NevaWebappsClient::GetSecurityLevelForWebContents(
    content::WebContents* web_contents) {
  // Security check is a simplified version comparing to ChromeWebappsClient.
  return security_state::GetSecurityLevel(
      *security_state::GetVisibleSecurityState(web_contents));
}

infobars::ContentInfoBarManager*
NevaWebappsClient::GetInfoBarManagerForWebContents(
    content::WebContents* web_contents) {
  return nullptr;
}

webapps::WebappInstallSource NevaWebappsClient::GetInstallSource(
    content::WebContents* web_contents,
    webapps::InstallTrigger trigger) {
  return webapps::WebappInstallSource::MENU_BROWSER_TAB;
}

webapps::AppBannerManager* NevaWebappsClient::GetAppBannerManager(
    content::WebContents* web_contents) {
  return nullptr;
}

bool NevaWebappsClient::IsOriginConsideredSecure(const url::Origin& url) {
  return true;
}

// The remainder of webapps::WebappsClient became pure virtual over M120..M151
// to serve Chrome's web app registry, its "seen manifest" cache and the ML
// install-promotion guardrails. None of those exist in app_runtime: installed
// apps are tracked by the platform app installer, not by webapps, so every
// query below answers "nothing known here" and every notification is dropped.
// This keeps the M120 behaviour, where WebappsClient never asked at all.

void NevaWebappsClient::DoesNewWebAppConflictWithExistingInstallation(
    content::BrowserContext* browser_context,
    const GURL& start_url,
    const webapps::ManifestId& manifest_id,
    WebAppInstallationConflictCallback callback) const {
  // Contract says the callback runs synchronously on desktop.
  std::move(callback).Run(false);
}

bool NevaWebappsClient::IsInAppBrowsingContext(
    content::WebContents* web_contents) const {
  return false;
}

bool NevaWebappsClient::IsAppPartiallyInstalledForSiteUrl(
    content::BrowserContext* browsing_context,
    const GURL& site_url) const {
  return false;
}

bool NevaWebappsClient::IsAppFullyInstalledForSiteUrl(
    content::BrowserContext* browsing_context,
    const GURL& site_url) const {
  return false;
}

bool NevaWebappsClient::IsUrlControlledBySeenManifest(
    content::BrowserContext* browsing_context,
    const GURL& site_url) const {
  return false;
}

void NevaWebappsClient::OnManifestSeen(
    content::BrowserContext* browsing_context,
    const blink::mojom::Manifest& manifest) const {}

void NevaWebappsClient::SaveInstallationIgnoredForMl(
    content::BrowserContext* browsing_context,
    const GURL& manifest_id) const {}

void NevaWebappsClient::SaveInstallationDismissedForMl(
    content::BrowserContext* browsing_context,
    const GURL& manifest_id) const {}

void NevaWebappsClient::SaveInstallationAcceptedForMl(
    content::BrowserContext* browsing_context,
    const GURL& manifest_id) const {}

bool NevaWebappsClient::IsMlPromotionBlockedByHistoryGuardrail(
    content::BrowserContext* browsing_context,
    const GURL& manifest_id) const {
  return false;
}

segmentation_platform::SegmentationPlatformService*
NevaWebappsClient::GetSegmentationPlatformService(
    content::BrowserContext* browsing_context) const {
  return nullptr;
}

std::optional<webapps::AppId> NevaWebappsClient::GetAppIdForWebContents(
    content::WebContents* web_contents) {
  return std::nullopt;
}

}  // namespace neva_app_runtime
