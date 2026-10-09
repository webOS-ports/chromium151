// Copyright 2017-2018 LG Electronics, Inc.
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

#include "webos/webview_profile.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "neva/app_runtime/webview_profile.h"
#include "url/gurl.h"

namespace webos {

WebViewProfile::WebViewProfile(const std::string& storage_name)
    : profile_(new neva_app_runtime::WebViewProfile(storage_name)),
      is_default_(false) {}

WebViewProfile::WebViewProfile(neva_app_runtime::WebViewProfile* default_profile)
    : profile_(default_profile), is_default_(true) {}

WebViewProfile* WebViewProfile::GetDefaultProfile() {
  static WebViewProfile* profile =
      new WebViewProfile(neva_app_runtime::WebViewProfile::GetDefaultProfile());
  return profile;
}

WebViewProfile* WebViewProfile::GetAlternativeProfile() {
  static WebViewProfile* profile =
      new WebViewProfile(neva_app_runtime::WebViewProfile::GetAlternativeProfile());
  return profile;
}

WebViewProfile::~WebViewProfile() {
  if (!is_default_)
    delete profile_;
}

void WebViewProfile::AppendExtraWebSocketHeader(const std::string& key,
                                                const std::string& value) {
  profile_->AppendExtraWebSocketHeader(key, value);
}

void WebViewProfile::FlushCookieStore() {
  profile_->FlushCookieStore();
}

void WebViewProfile::RemoveBrowsingData(int remove_browsing_data_mask) {
  profile_->RemoveBrowsingData(remove_browsing_data_mask);
}

void WebViewProfile::SetNotifierEnabled(const std::string& app_id,
                                        bool enabled) {
  GURL origin = GURL("file:///");
  origin.set_webapp_id(app_id);
  VLOG(1) << __func__ << " origin: " << origin;
  profile_->SetNotifierEnabled(origin, enabled);
}

void WebViewProfile::ResetNotifier(const std::string& app_id) {
  GURL origin = GURL("file:///");
  origin.set_webapp_id(app_id);
  VLOG(1) << __func__ << " origin: " << origin;
  profile_->ResetNotifier(origin);
}

namespace {

neva_app_runtime::WebViewProfile::AppPermission ToNeva(
    WebViewProfile::AppPermission permission) {
  switch (permission) {
    case WebViewProfile::AppPermission::kGeolocation:
      return neva_app_runtime::WebViewProfile::AppPermission::kGeolocation;
    case WebViewProfile::AppPermission::kNotifications:
      return neva_app_runtime::WebViewProfile::AppPermission::kNotifications;
  }
  NOTREACHED();
}

neva_app_runtime::WebViewProfile::AppPermissionSetting ToNeva(
    WebViewProfile::AppPermissionSetting setting) {
  switch (setting) {
    case WebViewProfile::AppPermissionSetting::kAsk:
      return neva_app_runtime::WebViewProfile::AppPermissionSetting::kAsk;
    case WebViewProfile::AppPermissionSetting::kAllow:
      return neva_app_runtime::WebViewProfile::AppPermissionSetting::kAllow;
    case WebViewProfile::AppPermissionSetting::kBlock:
      return neva_app_runtime::WebViewProfile::AppPermissionSetting::kBlock;
  }
  NOTREACHED();
}

WebViewProfile::AppPermissionSetting FromNeva(
    neva_app_runtime::WebViewProfile::AppPermissionSetting setting) {
  switch (setting) {
    case neva_app_runtime::WebViewProfile::AppPermissionSetting::kAsk:
      return WebViewProfile::AppPermissionSetting::kAsk;
    case neva_app_runtime::WebViewProfile::AppPermissionSetting::kAllow:
      return WebViewProfile::AppPermissionSetting::kAllow;
    case neva_app_runtime::WebViewProfile::AppPermissionSetting::kBlock:
      return WebViewProfile::AppPermissionSetting::kBlock;
  }
  NOTREACHED();
}

}  // namespace

std::map<std::string, WebViewProfile::AppPermissionSetting>
WebViewProfile::GetAppPermissions(AppPermission permission) {
  std::map<std::string, AppPermissionSetting> result;
  for (const auto& [app_id, setting] :
       profile_->GetAppPermissions(ToNeva(permission))) {
    result.emplace(app_id, FromNeva(setting));
  }
  return result;
}

void WebViewProfile::SetAppPermission(const std::string& app_id,
                                      AppPermission permission,
                                      AppPermissionSetting setting) {
  profile_->SetAppPermission(app_id, ToNeva(permission), ToNeva(setting));
}

void WebViewProfile::ResetAppPermissions(AppPermission permission) {
  profile_->ResetAppPermissions(ToNeva(permission));
}

}  // namespace webos
