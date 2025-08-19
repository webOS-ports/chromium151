// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Referred from chrome/browser/media/webrtc/media_capture_devices_dispatcher.cc

#include "neva/app_runtime/browser/media/webrtc/media_capture_devices_dispatcher.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "components/webrtc/media_stream_devices_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_capture_devices.h"
#include "content/public/browser/web_contents.h"
#include "neva/app_runtime/browser/media/webrtc/device_media_stream_access_handler.h"
#include "neva/app_runtime/browser/media/webrtc/media_stream_capture_indicator.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace neva_app_runtime {

namespace {

const char kDefaultAudioCaptureDevice[] = "media.default_audio_capture_device";
const char kDefaultVideoCaptureDevice[] = "media.default_video_capture_Device";

content::WebContents* WebContentsFromIds(int render_process_id,
                                         int render_frame_id) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(render_process_id, render_frame_id));
  return web_contents;
}

}  // namespace

MediaCaptureDevicesDispatcher* MediaCaptureDevicesDispatcher::GetInstance() {
  return base::Singleton<MediaCaptureDevicesDispatcher>::get();
}

MediaCaptureDevicesDispatcher::MediaCaptureDevicesDispatcher()
    : media_stream_capture_indicator_(new MediaStreamCaptureIndicator()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  media_access_handlers_.push_back(
      std::make_unique<DeviceMediaStreamAccessHandler>());
}

MediaCaptureDevicesDispatcher::~MediaCaptureDevicesDispatcher() = default;

void MediaCaptureDevicesDispatcher::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kDefaultAudioCaptureDevice, std::string());
  registry->RegisterStringPref(kDefaultVideoCaptureDevice, std::string());
}

void MediaCaptureDevicesDispatcher::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& handler : media_access_handlers_) {
    if (handler->SupportsStreamType(web_contents, request.video_type) ||
        handler->SupportsStreamType(web_contents, request.audio_type)) {
      handler->HandleRequest(web_contents, request, std::move(callback));
      return;
    }
  }
  std::move(callback).Run(blink::mojom::StreamDevicesSet(),
                          blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED,
                          nullptr);
}

bool MediaCaptureDevicesDispatcher::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& handler : media_access_handlers_) {
    if (handler->SupportsStreamType(
            content::WebContents::FromRenderFrameHost(render_frame_host),
            type)) {
      return handler->CheckMediaAccessPermission(render_frame_host,
                                                 security_origin, type);
    }
  }
  return false;
}

std::string MediaCaptureDevicesDispatcher::GetDefaultDeviceIDForProfile(
    content::BrowserContext* context,
    blink::mojom::MediaStreamType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  if (type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE)
    return prefs->GetString(kDefaultAudioCaptureDevice);
  else if (type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE)
    return prefs->GetString(kDefaultVideoCaptureDevice);
  else
    return std::string();
}

const blink::MediaStreamDevices&
MediaCaptureDevicesDispatcher::GetAudioCaptureDevices() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::MediaCaptureDevices::GetInstance()->GetAudioCaptureDevices();
}

const blink::MediaStreamDevices&
MediaCaptureDevicesDispatcher::GetVideoCaptureDevices() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::MediaCaptureDevices::GetInstance()->GetVideoCaptureDevices();
}

namespace {

// Picks the device named by `pref_name`, falling back to the first entry.
// Replaces the GetRequestedAudio/VideoDevice() helpers M151 removed along with
// the old MediaStreamDeviceEnumerator interface.
const std::optional<blink::MediaStreamDevice> PreferredDevice(
    content::BrowserContext* context,
    const char* pref_name,
    blink::MediaStreamDevices devices,
    const std::vector<std::string>& eligible_device_ids) {
  if (!eligible_device_ids.empty())
    devices = webrtc::FilterMediaDevices(devices, eligible_device_ids);
  if (devices.empty())
    return std::nullopt;

  const std::string default_device =
      user_prefs::UserPrefs::Get(context)->GetString(pref_name);
  for (const blink::MediaStreamDevice& device : devices) {
    if (device.id == default_device)
      return device;
  }
  return devices.front();
}

}  // namespace

const std::optional<blink::MediaStreamDevice>
MediaCaptureDevicesDispatcher::GetPreferredAudioDeviceForBrowserContext(
    content::BrowserContext* context,
    const std::vector<std::string>& eligible_device_ids) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return PreferredDevice(context, kDefaultAudioCaptureDevice,
                         GetAudioCaptureDevices(), eligible_device_ids);
}

const std::optional<blink::MediaStreamDevice>
MediaCaptureDevicesDispatcher::GetPreferredVideoDeviceForBrowserContext(
    content::BrowserContext* context,
    const std::vector<std::string>& eligible_device_ids) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return PreferredDevice(context, kDefaultVideoCaptureDevice,
                         GetVideoCaptureDevices(), eligible_device_ids);
}

scoped_refptr<MediaStreamCaptureIndicator>
MediaCaptureDevicesDispatcher::GetMediaStreamCaptureIndicator() {
  return media_stream_capture_indicator_;
}

void MediaCaptureDevicesDispatcher::OnMediaRequestStateChanged(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const GURL& security_origin,
    blink::mojom::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaCaptureDevicesDispatcher::UpdateMediaRequestStateOnUIThread,
          base::Unretained(this), render_process_id, render_frame_id,
          page_request_id, security_origin, stream_type, state));
}

void MediaCaptureDevicesDispatcher::OnSetCapturingLinkSecured(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    bool is_secure) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!blink::IsVideoScreenCaptureMediaType(stream_type))
    return;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaCaptureDevicesDispatcher::UpdateVideoScreenCaptureStatus,
          base::Unretained(this), render_process_id, render_frame_id,
          page_request_id, stream_type, is_secure));
}

bool MediaCaptureDevicesDispatcher::IsInsecureCapturingInProgress(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& handler : media_access_handlers_) {
    if (handler->IsInsecureCapturingInProgress(render_process_id,
                                               render_frame_id))
      return true;
  }

  return false;
}

void MediaCaptureDevicesDispatcher::UpdateMediaRequestStateOnUIThread(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const GURL& security_origin,
    blink::mojom::MediaStreamType stream_type,
    content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (const auto& handler : media_access_handlers_) {
    if (handler->SupportsStreamType(
            WebContentsFromIds(render_process_id, render_frame_id),
            stream_type)) {
      handler->UpdateMediaRequestState(render_process_id, render_frame_id,
                                       page_request_id, stream_type, state);
      break;
    }
  }
}

void MediaCaptureDevicesDispatcher::UpdateVideoScreenCaptureStatus(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    blink::mojom::MediaStreamType stream_type,
    bool is_secure) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(blink::IsVideoScreenCaptureMediaType(stream_type));

  for (const auto& handler : media_access_handlers_) {
    if (handler->SupportsStreamType(
            WebContentsFromIds(render_process_id, render_frame_id),
            stream_type)) {
      handler->UpdateVideoScreenCaptureStatus(
          render_process_id, render_frame_id, page_request_id, is_secure);
      break;
    }
  }
}

}  // namespace neva_app_runtime
