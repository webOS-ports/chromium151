// Copyright 2019 LG Electronics, Inc.
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

#ifndef NEVA_APP_RUNTIME_BROWSER_APP_RUNTIME_SHARED_MEMORY_MANAGER_H_
#define NEVA_APP_RUNTIME_BROWSER_APP_RUNTIME_SHARED_MEMORY_MANAGER_H_

#include "base/compiler_specific.h"
#include "base/memory/memory_pressure_listener.h"

namespace discardable_memory {
class DiscardableSharedMemoryManager;
}  // namespace discardable_memory

namespace neva_app_runtime {

class AppRuntimeSharedMemoryManager : public base::MemoryPressureListener {
 public:
  AppRuntimeSharedMemoryManager();
  AppRuntimeSharedMemoryManager(const AppRuntimeSharedMemoryManager&) = delete;
  AppRuntimeSharedMemoryManager& operator=(
      const AppRuntimeSharedMemoryManager&) = delete;
  ~AppRuntimeSharedMemoryManager();

  // base::MemoryPressureListener:
  void OnMemoryPressure(
      base::MemoryPressureLevel memory_pressure_level) override;

 private:

  size_t memory_pressure_divider_ = 4;
  size_t minimal_limit_ = 8 * 1024 * 1024;
  size_t memory_limit_;
  // Registers this listener for the lifetime of the object. The tag is an
  // upstream allowlist with no neva entry; kDiscardableSharedMemoryManager is
  // what this class actually drives.
  base::MemoryPressureListenerRegistration
      memory_pressure_listener_registration_;
  discardable_memory::DiscardableSharedMemoryManager*
      discardable_shared_memory_manager_ = nullptr;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_APP_RUNTIME_SHARED_MEMORY_MANAGER_H_
