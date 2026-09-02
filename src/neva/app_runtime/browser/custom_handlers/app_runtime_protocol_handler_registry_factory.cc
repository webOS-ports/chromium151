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

// Based on
// //components/custom_handlers/simple_protocol_handler_registry_factory.cc

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_runtime/browser/custom_handlers/app_runtime_protocol_handler_registry_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "neva/app_runtime/browser/custom_handlers/app_runtime_protocol_handler_registry_delegate.h"

namespace neva_app_runtime {

// static
AppRuntimeProtocolHandlerRegistryFactory*
AppRuntimeProtocolHandlerRegistryFactory::GetInstance() {
  static base::NoDestructor<AppRuntimeProtocolHandlerRegistryFactory> factory;
  return factory.get();
}

// static
custom_handlers::ProtocolHandlerRegistry*
AppRuntimeProtocolHandlerRegistryFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<custom_handlers::ProtocolHandlerRegistry*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AppRuntimeProtocolHandlerRegistryFactory::
    AppRuntimeProtocolHandlerRegistryFactory()
    : BrowserContextKeyedServiceFactory(
          "ProtocolHandlerRegistry",
          BrowserContextDependencyManager::GetInstance()) {}

// Will be created when initializing profile_io_data, so we might
// as well have the framework create this along with other
// PKSs to preserve orderly civic conduct :)
bool AppRuntimeProtocolHandlerRegistryFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

// Do not create this service for tests. MANY tests will fail
// due to the threading requirements of this service. ALSO,
// not creating this increases test isolation (which is GOOD!)
bool AppRuntimeProtocolHandlerRegistryFactory::ServiceIsNULLWhileTesting()
    const {
  return true;
}

// Off-the-record contexts get their own registry rather than none.
//
// BrowserContextKeyedServiceFactory's default returns nullptr for an
// off-the-record context, and nothing here overrode it. That was survivable
// until M151 added extensions/browser/api/protocol_handlers, whose
// ProtocolHandlersSanityCheck() and OnExtensionUnloaded() treat a null
// registry as reachable only from a test and CHECK_IS_TEST() on it. Browser
// shell builds an off-the-record context for its private partition, so the
// check fires there and aborts the process as soon as the first tab is
// created - which is every launch of the Enact browser.
//
// Chrome's own ProtocolHandlerRegistryFactory has always returned the
// context's own instance in incognito for the same reason; this is that,
// without the //chrome helper. The registry is told it is off the record so
// it keeps handlers in memory instead of persisting them.
content::BrowserContext*
AppRuntimeProtocolHandlerRegistryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

std::unique_ptr<KeyedService>
AppRuntimeProtocolHandlerRegistryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // We can't ensure the UserPref has been set, so we pass a nullptr
  // PrefService.
  return custom_handlers::ProtocolHandlerRegistry::Create(
      nullptr, std::make_unique<AppRuntimeProtocolHandlerRegistryDelegate>(),
      context->IsOffTheRecord());
}

}  // namespace neva_app_runtime
