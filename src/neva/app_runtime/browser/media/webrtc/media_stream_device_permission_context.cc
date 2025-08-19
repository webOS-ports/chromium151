// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied from
// chrome/browser/media/webrtc/media_stream_device_permission_context.cc

#include "base/logging.h"
#include "neva/app_runtime/browser/media/webrtc/media_stream_device_permission_context.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

namespace {

network::mojom::PermissionsPolicyFeature GetPermissionsPolicyFeature(
    ContentSettingsType type) {
  if (type == ContentSettingsType::MEDIASTREAM_MIC)
    return network::mojom::PermissionsPolicyFeature::kMicrophone;

  DCHECK_EQ(ContentSettingsType::MEDIASTREAM_CAMERA, type);
  return network::mojom::PermissionsPolicyFeature::kCamera;
}

}  // namespace

namespace neva_app_runtime {

MediaStreamDevicePermissionContext::MediaStreamDevicePermissionContext(
    content::BrowserContext* browser_context,
    ContentSettingsType content_settings_type)
    : PermissionContextBase(browser_context,
                            content_settings_type,
                            GetPermissionsPolicyFeature(content_settings_type)),
      content_settings_type_(content_settings_type) {
  DCHECK(content_settings_type_ == ContentSettingsType::MEDIASTREAM_MIC ||
         content_settings_type_ == ContentSettingsType::MEDIASTREAM_CAMERA);
}

MediaStreamDevicePermissionContext::~MediaStreamDevicePermissionContext() {}

void MediaStreamDevicePermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    permissions::BrowserPermissionCallback callback) {
  // PermissionRequestManager::CreateForWebContents must have been called
  // during web contents  initialization.
  permissions::PermissionContextBase::DecidePermission(std::move(request_data),
                                                       std::move(callback));
}

PermissionSetting MediaStreamDevicePermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  PermissionSetting setting =
      permissions::PermissionContextBase::GetPermissionStatusInternal(
          render_frame_host, requesting_origin, embedding_origin);

  VLOG(2) << __func__ << " requesting_origin=" << requesting_origin
          << ", embedding_origin=" << embedding_origin
          << ", setting=" << setting;
  if (auto* content_setting = std::get_if<ContentSetting>(&setting);
      content_setting && *content_setting == CONTENT_SETTING_DEFAULT) {
    return CONTENT_SETTING_ASK;
  }

  return setting;
}

void MediaStreamDevicePermissionContext::ResetPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  NOTREACHED() << "ResetPermission is not implemented";
}

bool MediaStreamDevicePermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}

}  // namespace neva_app_runtime
