// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Based on chrome/browser/permissions/permission_actions_history_factory.cc

#include "neva/app_runtime/browser/permissions/permission_actions_history_factory.h"

#include <memory>

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/permission_actions_history.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "neva/app_runtime/browser/host_content_settings_map_factory.h"

namespace neva_app_runtime {

// static
permissions::PermissionActionsHistory*
PermissionActionsHistoryFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<permissions::PermissionActionsHistory*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
PermissionActionsHistoryFactory*
PermissionActionsHistoryFactory::GetInstance() {
  static base::NoDestructor<PermissionActionsHistoryFactory> factory;
  return factory.get();
}

PermissionActionsHistoryFactory::PermissionActionsHistoryFactory()
    : BrowserContextKeyedServiceFactory(
          "PermissionActionsHistory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PermissionActionsHistoryFactory::~PermissionActionsHistoryFactory() = default;

std::unique_ptr<KeyedService>
PermissionActionsHistoryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<permissions::PermissionActionsHistory>(
      user_prefs::UserPrefs::Get(context),
      HostContentSettingsMapFactory::GetForBrowserContext(context));
}

content::BrowserContext*
PermissionActionsHistoryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace neva_app_runtime
