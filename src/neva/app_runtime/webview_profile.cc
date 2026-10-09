// Copyright 2017 LG Electronics, Inc.
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

#include "neva/app_runtime/webview_profile.h"

#include <memory>
#include <optional>
#include <variant>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "browser/browsing_data/browsing_data_remover.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_context_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/neva/proxy_settings.h"
#include "neva/app_runtime/app/app_runtime_main_delegate.h"
#include "neva/app_runtime/browser/app_runtime_browser_context.h"
#include "neva/app_runtime/browser/host_content_settings_map_factory.h"
#include "neva/app_runtime/public/file_security_origin.h"
#include "neva/app_runtime/public/notifier_settings_controller.h"

namespace neva_app_runtime {

namespace {

ContentSettingsType ToContentSettingsType(
    WebViewProfile::AppPermission permission) {
  switch (permission) {
    case WebViewProfile::AppPermission::kGeolocation:
      // The type the geolocation permission context stores its decisions
      // under; it depends on a feature, so ask the same function it does.
      return content_settings::GeolocationContentSettingsType();
    case WebViewProfile::AppPermission::kNotifications:
      return ContentSettingsType::NOTIFICATIONS;
  }
  NOTREACHED();
}

// The URL a permission context files an application's decisions under: the
// file:// origin with the application's host (see
// PermissionContextBase::convertToApplicationURL).
GURL ApplicationURL(const std::string& app_id) {
  GURL origin("file:///");
  origin.set_webapp_id(app_id);
  return permissions::PermissionContextBase::convertToApplicationURL(origin);
}

// The primary pattern the decisions for |app_id| are stored under.
ContentSettingsPattern PatternForApp(const std::string& app_id,
                                     ContentSettingsType type) {
  return HostContentSettingsMap::GetPatternsForContentSettingsType(
             ApplicationURL(app_id), GURL(), type)
      .first;
}

// The application a stored rule belongs to, or nothing for a rule that is not
// exactly one application's: the default rule, a wildcard, a site pattern. The
// host is FileSchemeHostForApp(app_id), which lowercases the id, so the id
// comes back lowercase.
std::optional<std::string> AppIdForRule(const ContentSettingPatternSource& rule,
                                        ContentSettingsType type) {
  if (rule.source != content_settings::mojom::ProviderType::kPrefProvider ||
      rule.secondary_pattern != ContentSettingsPattern::Wildcard()) {
    return std::nullopt;
  }

  const std::string& host = rule.primary_pattern.GetHost();
  // FileSchemeHostForApp("") is just the suffix every application host ends
  // with.
  const std::string suffix = FileSchemeHostForApp(std::string());
  if (host.size() <= suffix.size() ||
      !base::EndsWith(host, suffix, base::CompareCase::SENSITIVE)) {
    return std::nullopt;
  }

  std::string app_id = host.substr(0, host.size() - suffix.size());
  // Only a rule the setters below would address is one application's.
  if (PatternForApp(app_id, type) != rule.primary_pattern) {
    return std::nullopt;
  }
  return app_id;
}

std::optional<WebViewProfile::AppPermissionSetting> ToAppPermissionSetting(
    const PermissionSetting& setting) {
  if (const ContentSetting* content_setting =
          std::get_if<ContentSetting>(&setting)) {
    switch (*content_setting) {
      case CONTENT_SETTING_ALLOW:
        return WebViewProfile::AppPermissionSetting::kAllow;
      case CONTENT_SETTING_BLOCK:
        return WebViewProfile::AppPermissionSetting::kBlock;
      default:
        return std::nullopt;
    }
  }
  const GeolocationSetting& geolocation = std::get<GeolocationSetting>(setting);
  if (geolocation.precise == PermissionOption::kAllowed ||
      geolocation.approximate == PermissionOption::kAllowed) {
    return WebViewProfile::AppPermissionSetting::kAllow;
  }
  if (geolocation.precise == PermissionOption::kDenied &&
      geolocation.approximate == PermissionOption::kDenied) {
    return WebViewProfile::AppPermissionSetting::kBlock;
  }
  return std::nullopt;
}

// What the permission prompt would have stored for |setting|, in the form
// |type| keeps.
PermissionSetting ToPermissionSetting(
    WebViewProfile::AppPermissionSetting setting,
    ContentSettingsType type) {
  const bool allow = setting == WebViewProfile::AppPermissionSetting::kAllow;
  if (type == ContentSettingsType::GEOLOCATION_WITH_OPTIONS) {
    const PermissionOption option =
        allow ? PermissionOption::kAllowed : PermissionOption::kDenied;
    return GeolocationSetting{option, option};
  }
  return allow ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
}

}  // namespace

WebViewProfile::WebViewProfile(const std::string& storage_name)
    : browser_context_(AppRuntimeBrowserContext::From(storage_name)) {}

AppRuntimeBrowserContext* WebViewProfile::GetBrowserContext() const {
  return browser_context_;
}

WebViewProfile* WebViewProfile::GetDefaultProfile() {
  static base::NoDestructor<std::unique_ptr<WebViewProfile>> profile(
      new WebViewProfile(AppRuntimeBrowserContext::From("")));
  return (*profile).get();
}

WebViewProfile* WebViewProfile::GetAlternativeProfile() {
  static base::NoDestructor<std::unique_ptr<WebViewProfile>> profile(
      new WebViewProfile(AppRuntimeBrowserContext::From("private")));
  return (*profile).get();
}

void WebViewProfile::SetProxyServer(
    const content::ProxySettings& proxy_settings) {
  GetAppRuntimeContentBrowserClient()->SetProxyServer(proxy_settings);
}

void WebViewProfile::AppendExtraWebSocketHeader(const std::string& key,
                                                const std::string& value) {
  GetAppRuntimeContentBrowserClient()->AppendExtraWebSocketHeader(key, value);
}

void WebViewProfile::RemoveBrowsingData(int remove_browsing_data_mask,
                                        const GURL& origin,
                                        base::OnceCallback<void()> callback) {
  BrowsingDataRemover* remover = BrowsingDataRemover::GetForStoragePartition(
      browser_context_->GetDefaultStoragePartition());
  remover->Remove(BrowsingDataRemover::Unbounded(), remove_browsing_data_mask,
                  origin, std::move(callback));
}

void WebViewProfile::RemoveBrowsingData(int remove_browsing_data_mask) {
  BrowsingDataRemover* remover = BrowsingDataRemover::GetForStoragePartition(
      browser_context_->GetDefaultStoragePartition());
  remover->Remove(BrowsingDataRemover::Unbounded(), remove_browsing_data_mask,
                  GURL(), base::DoNothing());
}

void WebViewProfile::FlushCookieStore() {
  browser_context_->FlushCookieStore();
}

void WebViewProfile::SetNotifierEnabled(const GURL& origin, bool enabled) {
  NotifierSettingsController* controller =
      browser_context_->GetNotifierSettingsController();
  if (controller)
    controller->SetNotifierEnabled(origin, enabled);
}

void WebViewProfile::ResetNotifier(const GURL& origin) {
  NotifierSettingsController* controller =
      browser_context_->GetNotifierSettingsController();
  if (controller)
    controller->ResetNotifier(origin);
}

WebViewProfile::WebViewProfile(AppRuntimeBrowserContext* browser_context)
    : browser_context_(browser_context) {}

std::map<std::string, WebViewProfile::AppPermissionSetting>
WebViewProfile::GetAppPermissions(AppPermission permission) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::map<std::string, AppPermissionSetting> result;
  const HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForBrowserContext(browser_context_);
  if (!map) {
    return result;
  }

  const ContentSettingsType type = ToContentSettingsType(permission);
  for (const ContentSettingPatternSource& rule :
       map->GetSettingsForOneType(type)) {
    std::optional<std::string> app_id = AppIdForRule(rule, type);
    if (!app_id) {
      continue;
    }
    // Read back through the map, as the permission context does, rather than
    // decoding the rule's value: the map knows the form |type| stores.
    std::optional<AppPermissionSetting> setting = ToAppPermissionSetting(
        map->GetPermissionSetting(ApplicationURL(*app_id), GURL(), type));
    if (setting) {
      result[*app_id] = *setting;
    }
  }
  return result;
}

void WebViewProfile::SetAppPermission(const std::string& app_id,
                                      AppPermission permission,
                                      AppPermissionSetting setting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForBrowserContext(browser_context_);
  if (!map || app_id.empty()) {
    return;
  }

  const ContentSettingsType type = ToContentSettingsType(permission);
  // The same call, URL and scope the permission context uses when the user
  // answers, so the page sees this on its next request. Ask removes the rule.
  map->SetPermissionSettingDefaultScope(
      ApplicationURL(app_id), GURL(), type,
      setting == AppPermissionSetting::kAsk
          ? std::nullopt
          : std::optional<PermissionSetting>(
                ToPermissionSetting(setting, type)));
}

void WebViewProfile::ResetAppPermissions(AppPermission permission) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Collected first: removing rules while walking the map's own list would
  // walk a list that changes underneath. Everything runs on the UI thread, so
  // nothing else can add or change a rule in between.
  for (const auto& [app_id, setting] : GetAppPermissions(permission)) {
    SetAppPermission(app_id, permission, AppPermissionSetting::kAsk);
  }
}

}  // namespace neva_app_runtime
