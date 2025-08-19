// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_runtime/browser/geolocation/geolocation_permission_context.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"

namespace neva_app_runtime {

GeolocationPermissionContext::GeolocationPermissionContext(
    content::BrowserContext* browser_context)
    : permissions::GeolocationPermissionContext(browser_context, nullptr) {}

GeolocationPermissionContext::~GeolocationPermissionContext() = default;

void GeolocationPermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    permissions::BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PermissionContextBase::DecidePermission(std::move(request_data),
                                          std::move(callback));
}

PermissionSetting GeolocationPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PermissionSetting setting =
      permissions::PermissionContextBase::GetPermissionStatusInternal(
          render_frame_host, requesting_origin, embedding_origin);

  // M151 widened this to a variant. Only the ContentSetting alternative gets
  // the DEFAULT -> ASK treatment; a GeolocationSetting passes through.
  if (auto* content_setting = std::get_if<ContentSetting>(&setting);
      content_setting && *content_setting == CONTENT_SETTING_DEFAULT) {
    return CONTENT_SETTING_ASK;
  }

  return setting;
}

}  // namespace neva_app_runtime
