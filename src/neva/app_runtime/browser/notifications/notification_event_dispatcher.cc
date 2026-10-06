// Copyright 2022 LG Electronics, Inc.
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

#include "neva/app_runtime/browser/notifications/notification_event_dispatcher.h"
#include "base/notimplemented.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "content/browser/notifications/notification_event_dispatcher_impl.h"
#include "content/browser/notifications/notification_id_generator.h"
#include "neva/app_runtime/browser/app_runtime_browser_context.h"
#include "neva/app_runtime/webview_profile.h"
namespace neva_app_runtime {

NotificationEventDispatcher* GetNotificationEventDispatcher() {
  return NotificationEventDispatcherImpl::GetInstance();
}

NotificationEventDispatcherImpl*
NotificationEventDispatcherImpl::GetInstance() {
  return base::Singleton<NotificationEventDispatcherImpl>::get();
}

NotificationEventDispatcherImpl::NotificationEventDispatcherImpl() = default;

NotificationEventDispatcherImpl::~NotificationEventDispatcherImpl() = default;

void NotificationEventDispatcherImpl::Close(const std::string& notification_id,
                                            bool by_user) {
  if (content::NotificationIdGenerator::IsNonPersistentNotification(
          notification_id)) {
    content::NotificationEventDispatcherImpl::GetInstance()
        ->DispatchNonPersistentCloseEvent(notification_id, base::DoNothing());
    return;
  }
  // A service worker's notificationclose event needs the origin, which this
  // interface does not carry.
  NOTIMPLEMENTED();
}

void NotificationEventDispatcherImpl::Click(
    const std::string& notification_id,
    const std::string origin,
    const std::pair<int, bool>& button_index,
    const std::pair<std::u16string, bool>& reply) {
  // A notification made with new Notification() has no service worker; its
  // click goes to the page that made it.
  if (content::NotificationIdGenerator::IsNonPersistentNotification(
          notification_id)) {
    content::NotificationEventDispatcherImpl::GetInstance()
        ->DispatchNonPersistentClickEvent(notification_id, base::DoNothing());
    return;
  }

  std::optional<int> index = std::nullopt;
  if (button_index.second) {
    index = button_index.first;
  }

  std::optional<std::u16string> text = std::nullopt;
  if (reply.second) {
    text = reply.first;
  }

  GURL gurl(origin);

  neva_app_runtime::WebViewProfile* profile =
      neva_app_runtime::WebViewProfile::GetDefaultProfile();

  content::NotificationEventDispatcherImpl::GetInstance()
      ->DispatchNotificationClickEvent(profile->GetBrowserContext(),
                                       notification_id, gurl, index, text,
                                       base::DoNothing());
}

}  // namespace neva_app_runtime
