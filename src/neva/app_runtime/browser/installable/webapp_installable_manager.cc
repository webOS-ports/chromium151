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

#include "neva/app_runtime/browser/installable/webapp_installable_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "neva/app_runtime/browser/installable/neva_webapps_client.h"
#include "neva/pal_service/pal_platform_factory.h"
#include "neva/pal_service/public/webapp_installable_delegate.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"

namespace neva_app_runtime {

WebAppInstallableManager::WebAppInstallableManager()
    : data_retriever_(std::make_unique<web_app::WebAppDataRetriever>()),
      pal_installable_delegate_(
          pal::PlatformFactory::Get()->CreateWebAppInstallableDelegate()),
      weak_factory_(this) {
  NevaWebappsClient::Create();
}

WebAppInstallableManager::~WebAppInstallableManager() {}

void WebAppInstallableManager::CheckInstallability(
    content::WebContents* web_contents,
    CheckInstallabilityCallback callback) {
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents,
      base::BindOnce(&WebAppInstallableManager::OnCheckInstallability,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

// M151: CheckInstallabilityCallback no longer passes the manifest URL, and
// web_app::UpdateWebAppInfoFromManifest is gone (WebAppInstallInfo now requires
// a manifest id and start URL up front). All this code ever wanted from the
// install info was the start URL, which the manifest carries directly.
void WebAppInstallableManager::OnCheckInstallability(
    CheckInstallabilityCallback callback,
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode is_installable) {
  VLOG(1) << std::boolalpha << __func__
          << "() valid_manifest_for_web_app=" << valid_manifest_for_web_app
          << " installable=" << static_cast<int>(is_installable);
  bool installed = false;
  bool installable = false;
  if (!opt_manifest)
    return std::move(callback).Run(false, installed);

  if (valid_manifest_for_web_app) {
    installable =
        is_installable == webapps::InstallableStatusCode::NO_ERROR_DETECTED;
    pal_installable_delegate_->IsWebAppForUrlInstalled(
        opt_manifest->start_url,
        base::BindOnce(
            &WebAppInstallableManager::OnIsWebAppForUrlInstallability,
            weak_factory_.GetWeakPtr(), installable, std::move(callback)));
  }
}

void WebAppInstallableManager::OnIsWebAppForUrlInstallability(
    bool is_installable,
    CheckInstallabilityCallback callback,
    bool is_installed) {
  std::move(callback).Run(is_installable, is_installed);
}

void WebAppInstallableManager::InstallWebApp(content::WebContents* web_contents,
                                             InstallWebAppCallback callback) {
  if (!web_contents)
    return std::move(callback).Run(false);

  web_contents->GetPrimaryPage().GetManifest(base::BindOnce(
      &WebAppInstallableManager::OnDidGetManifest, weak_factory_.GetWeakPtr(),
      web_contents, std::move(callback)));
}

void WebAppInstallableManager::OnDidGetManifest(
    content::WebContents* web_contents,
    InstallWebAppCallback callback,
    blink::mojom::ManifestRequestResult result,
    const GURL& manifest_url,
    blink::mojom::ManifestPtr manifest) {
  if (!manifest || manifest_url.is_empty() ||
      blink::IsEmptyManifest(manifest)) {
    std::move(callback).Run(false);
    return;
  }

  if (is_processing_install_) {
    std::move(callback).Run(false);
    return;
  }
  is_processing_install_ = true;

  // M151: one job now covers what UpdateWebAppInfoFromManifest plus a separate
  // GetIcons/PopulateProductIcons pass used to do.
  install_info_job_ = web_app::ManifestToWebAppInstallInfoJob::CreateAndStart(
      *manifest, *data_retriever_, /*background_installation=*/true,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      web_contents->GetWeakPtr(), [](web_app::IconUrlSizeSet&) {},
      install_info_debug_data_,
      base::BindOnce(&WebAppInstallableManager::OnInstallInfoCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAppInstallableManager::OnInstallInfoCreated(
    InstallWebAppCallback callback,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info) {
  // The job has already populated the icons on `web_app_info`.
  bool install_result = false;
  if (web_app_info) {
    auto delegate_info = ConvertAppInfo(web_app_info.get());
    install_result =
        pal_installable_delegate_->SaveArtifacts(delegate_info.get());
  }

  std::move(callback).Run(install_result);
  is_processing_install_ = false;
}

void WebAppInstallableManager::UpdateApp() {
  pal_installable_delegate_->UpdateApp();
}

// Update
void WebAppInstallableManager::MaybeUpdate(content::WebContents* web_contents) {
  VLOG(1) << "Begin update steps of PWA app";
  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents,
      base::BindOnce(&WebAppInstallableManager::OnManifestForUpdate,
                     weak_factory_.GetWeakPtr(), web_contents));
}

void WebAppInstallableManager::OnManifestForUpdate(
    content::WebContents* web_contents,
    blink::mojom::ManifestPtr manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode is_installable) {
  VLOG(1) << "valid_manifest_for_web_app: " << valid_manifest_for_web_app
          << ", is_installable: " << static_cast<int>(is_installable);
  if (!manifest || !valid_manifest_for_web_app ||
      is_installable != webapps::InstallableStatusCode::NO_ERROR_DETECTED) {
    return;
  }

  // Icons are deferred: the update is only worth fetching them for once
  // ShouldAppForURLBeUpdated() has said yes, which is what the M120 flow did
  // by keeping GetIcons out of this step.
  web_app::WebAppInstallInfoConstructOptions options;
  options.defer_icon_fetching = true;
  install_info_job_ = web_app::ManifestToWebAppInstallInfoJob::CreateAndStart(
      *manifest, *data_retriever_, /*background_installation=*/true,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      web_contents->GetWeakPtr(), [](web_app::IconUrlSizeSet&) {},
      install_info_debug_data_,
      base::BindOnce(&WebAppInstallableManager::OnInstallInfoCreatedForUpdate,
                     weak_factory_.GetWeakPtr(), web_contents),
      std::move(options));
}

void WebAppInstallableManager::OnInstallInfoCreatedForUpdate(
    content::WebContents* web_contents,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info) {
  if (!web_app_info)
    return;

  pal_installable_delegate_->IsWebAppForUrlInstalled(
      web_app_info->start_url(),
      base::BindOnce(&WebAppInstallableManager::OnWebAppForUrlisUpdate,
                     weak_factory_.GetWeakPtr(), web_contents,
                     std::move(web_app_info)));
}

void WebAppInstallableManager::OnWebAppForUrlisUpdate(
    content::WebContents* web_contents,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
    bool is_installed) {
  if (is_installed) {
    pal_installable_delegate_->ShouldAppForURLBeUpdated(
        web_app_info->start_url(),
        base::BindOnce(&WebAppInstallableManager::OnShouldAppForURLBeUpdated,
                       weak_factory_.GetWeakPtr(), web_contents,
                       std::move(web_app_info)));
  } else {
    VLOG(1) << "Do not update because the app is not installed";
  }
}

void WebAppInstallableManager::OnShouldAppForURLBeUpdated(
    content::WebContents* web_contents,
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info,
    bool should_update) {
  if (!should_update) {
    VLOG(1) << "The app should not be updated now";
    return;
  }

  // Icons were deferred at manifest time; fetch them now that the update is
  // going ahead. The job populates them onto `web_app_info` in place.
  CHECK(install_info_job_);
  web_app::WebAppInstallInfo* info = web_app_info.get();
  install_info_job_->FetchIcons(
      *info, *web_contents,
      base::BindOnce(&WebAppInstallableManager::OnIconsFetchedForUpdate,
                     weak_factory_.GetWeakPtr(), std::move(web_app_info)));
}

void WebAppInstallableManager::OnIconsFetchedForUpdate(
    std::unique_ptr<web_app::WebAppInstallInfo> web_app_info) {
  // The job populated the icons onto `web_app_info` in place.
  pal_installable_delegate_->IsInfoChanged(
      ConvertAppInfo(web_app_info.get()),
      base::BindOnce(&WebAppInstallableManager::OnIsInfoChanged,
                     weak_factory_.GetWeakPtr(),
                     ConvertAppInfo(web_app_info.get())));
}

void WebAppInstallableManager::OnIsInfoChanged(
    std::unique_ptr<pal::WebAppInstallableDelegate::WebAppInfo>
        new_delegate_info,
    bool value,
    const std::string& version) {
  if (value) {
    VLOG(1) << "Proceed with updating the app";
    new_delegate_info->set_version(version);
    bool install_result =
        pal_installable_delegate_->SaveArtifacts(new_delegate_info.get(), true);
    VLOG(1) << "The app update install_result: " << install_result;
  } else {
    VLOG(1) << "Do not update the app because resources are not changed";
  }
}

std::unique_ptr<pal::WebAppInstallableDelegate::WebAppInfo>
WebAppInstallableManager::ConvertAppInfo(
    const web_app::WebAppInstallInfo* web_app_info) {
  // M151 stores the "any" icons in a base::flat_map; the pal delegate ABI is
  // fixed on std::map, so copy across rather than change the pal interface.
  const std::map<pal::WebAppInstallableDelegate::WebAppInfo::SquareSizePx,
                 SkBitmap>
      icons(web_app_info->icon_bitmaps.any.begin(),
            web_app_info->icon_bitmaps.any.end());
  return pal_installable_delegate_->GenerateAppInfo(
      base::UTF16ToUTF8(web_app_info->title.value()), icons,
      web_app_info->start_url(), web_app_info->background_color);
}
}  // namespace neva_app_runtime
