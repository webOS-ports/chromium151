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

#ifndef NEVA_APP_RUNTIME_BROWSER_INSTALLABLE_WEBAPP_INSTALLABLE_MANAGER_H_
#define NEVA_APP_RUNTIME_BROWSER_INSTALLABLE_WEBAPP_INSTALLABLE_MANAGER_H_

#include <memory>

#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "neva/pal_service/public/webapp_installable_delegate.h"

struct WebAppInstallInfo;

namespace neva_app_runtime {

class WebAppInstallableManager {
 public:
  WebAppInstallableManager();
  virtual ~WebAppInstallableManager();

  using CheckInstallabilityCallback =
      base::OnceCallback<void(bool installable, bool installed)>;
  void CheckInstallability(content::WebContents* web_contents,
                           CheckInstallabilityCallback callback);

  using InstallWebAppCallback = base::OnceCallback<void(bool success)>;
  void InstallWebApp(content::WebContents* web_contents,
                     InstallWebAppCallback callback);
  void MaybeUpdate(content::WebContents* web_contents);
  void UpdateApp();

 private:
  void OnCheckInstallability(CheckInstallabilityCallback callback,
                             blink::mojom::ManifestPtr opt_manifest,
                             bool valid_manifest_for_web_app,
                             webapps::InstallableStatusCode is_installable);
  void OnIsWebAppForUrlInstallability(bool is_installable,
                                      CheckInstallabilityCallback callback,
                                      bool is_installed);
  void OnWebAppForUrlisUpdate(
      content::WebContents* web_contents,
      std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
      bool is_installed);
  void OnInstallInfoCreated(
      InstallWebAppCallback callback,
      std::unique_ptr<web_app::WebAppInstallInfo> web_app_info);
  void OnDidGetManifest(content::WebContents* web_contents,
                        InstallWebAppCallback callback,
                        blink::mojom::ManifestRequestResult result,
                        const GURL& manifest_url,
                        blink::mojom::ManifestPtr manifest);
  std::unique_ptr<pal::WebAppInstallableDelegate::WebAppInfo> ConvertAppInfo(
      const web_app::WebAppInstallInfo* web_app_info);
  void OnManifestForUpdate(content::WebContents* web_contents,
                           blink::mojom::ManifestPtr opt_manifest,
                           bool valid_manifest_for_web_app,
                           webapps::InstallableStatusCode is_installable);
  void OnShouldAppForURLBeUpdated(
      content::WebContents* web_contents,
      std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
      bool should_update);
  void OnInstallInfoCreatedForUpdate(
      content::WebContents* web_contents,
      std::unique_ptr<web_app::WebAppInstallInfo> web_app_info);
  void OnIconsFetchedForUpdate(
      std::unique_ptr<web_app::WebAppInstallInfo> web_app_info);
  void OnIsInfoChanged(
      std::unique_ptr<pal::WebAppInstallableDelegate::WebAppInfo>
          new_delegate_info,
      bool value,
      const std::string& version);

  // TODO This flag is necessary to prevent the plant
  // from being called again when the first one has not yet
  // finished its work
  bool is_processing_install_ = false;
  std::unique_ptr<web_app::WebAppDataRetriever> data_retriever_;
  // Owns the in-flight manifest -> install-info conversion. Also holds the
  // debug data the job writes into, which nothing here consumes.
  std::unique_ptr<web_app::ManifestToWebAppInstallInfoJob> install_info_job_;
  base::DictValue install_info_debug_data_;
  std::unique_ptr<pal::WebAppInstallableDelegate> pal_installable_delegate_;
  base::WeakPtrFactory<WebAppInstallableManager> weak_factory_;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_INSTALLABLE_WEBAPP_INSTALLABLE_MANAGER_H_
