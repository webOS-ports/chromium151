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

#include "neva/pal_service/webos/network_error_page_controller_delegate_webos.h"
#include "base/notimplemented.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "neva/pal_service/luna/luna_names.h"
#include "neva/pal_service/webos/network_error_settings_app.h"
#include "neva/shell_shutdown/shell_shutdown.h"

namespace pal {
namespace webos {

const char kLaunchingMethod[] = "launch";
const char kParams[] = "params";
const char kDisplayId[] = "displayAffinity";

NetworkErrorPageControllerDelegateWebOS::
    NetworkErrorPageControllerDelegateWebOS() {
  luna::Client::Params params;
#if !defined(USE_WEBOS_STARFISH) && !defined(USE_WEBOS_SIGNAGE)
  params.name = luna::GetServiceNameWithRandSuffix(
      luna::service_name::kSettingsClient, "-");
#endif
  luna_client_ = CreateClient(params);
}

NetworkErrorPageControllerDelegateWebOS::
    ~NetworkErrorPageControllerDelegateWebOS() {}

void NetworkErrorPageControllerDelegateWebOS::LaunchNetworkSettings(
    int target_id,
    int display_id) {
  // The settings page is a whole app of its own, so the target picks the app
  // rather than travelling as a parameter (see network_error_settings_app.h).
  const std::string_view app_id = SettingsAppIdForTarget(target_id);
  if (app_id.empty())
    return;

  base::DictValue params;
  params.Set(kDisplayId, display_id);

  base::DictValue resource;
  resource.Set("id", app_id);
  resource.Set(kParams, std::move(params));

  std::string payload;
  base::JSONWriter::Write(base::Value(std::move(resource)), &payload);
  luna_client_->Call(
      luna::GetServiceURI(luna::service_uri::kApplicationManager,
                          kLaunchingMethod),
      std::move(payload),
      base::BindOnce(
          &NetworkErrorPageControllerDelegateWebOS::OnLaunchNetworkSettings,
          base::Unretained(this)),
      std::string("{}"));
}

void NetworkErrorPageControllerDelegateWebOS::CloseShellIfNeeded() {
  neva_app_runtime::CallShellShutdownIfAvailable();
}

void NetworkErrorPageControllerDelegateWebOS::OnLaunchNetworkSettings(
    luna::Client::ResponseStatus,
    unsigned,
    const std::string& json) {
  NOTIMPLEMENTED();
}

}  // namespace webos
}  // namespace pal
