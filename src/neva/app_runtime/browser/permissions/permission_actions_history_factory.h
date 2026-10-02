// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Based on chrome/browser/permissions/permission_actions_history_factory.h

#ifndef NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_FACTORY_H_
#define NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace permissions {
class PermissionActionsHistory;
}

namespace neva_app_runtime {

class PermissionActionsHistoryFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  PermissionActionsHistoryFactory(const PermissionActionsHistoryFactory&) =
      delete;
  PermissionActionsHistoryFactory& operator=(
      const PermissionActionsHistoryFactory&) = delete;

  static permissions::PermissionActionsHistory* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static PermissionActionsHistoryFactory* GetInstance();

 private:
  friend class base::NoDestructor<PermissionActionsHistoryFactory>;

  PermissionActionsHistoryFactory();
  ~PermissionActionsHistoryFactory() override;

  // BrowserContextKeyedServiceFactory
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_PERMISSIONS_PERMISSION_ACTIONS_HISTORY_FACTORY_H_
