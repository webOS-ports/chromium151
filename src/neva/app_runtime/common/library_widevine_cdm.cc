// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_runtime/common/library_widevine_cdm.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "components/cdm/common/cdm_manifest.h"
#include "media/base/cdm_capability.h"
#include "media/cdm/cdm_paths.h"  // nogncheck
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck

namespace neva_app_runtime {

namespace {

constexpr base::FilePath::CharType kSystemCdmBasePath[] =
    FILE_PATH_LITERAL("/var/lib/widevine/WidevineCdm");

}  // namespace

std::optional<content::CdmInfo> CreateLibraryWidevineCdmInfo(
    const base::FilePath& cdm_base_path) {
  const base::FilePath cdm_library_path =
      media::GetPlatformSpecificDirectory(cdm_base_path)
          .Append(base::GetNativeLibraryName(kWidevineCdmLibraryName));
  if (!base::PathExists(cdm_library_path)) {
    return std::nullopt;
  }

  media::CdmCapability capability;
  if (!ParseCdmManifestFromPath(
          cdm_base_path.Append(FILE_PATH_LITERAL("manifest.json")),
          &capability)) {
    LOG(WARNING) << "Ignoring Widevine CDM with a bad manifest in "
                 << cdm_base_path;
    return std::nullopt;
  }

  // Software secure only: the module runs in a normal process, so nothing here
  // can honestly claim a hardware protected path.
  return content::CdmInfo(kWidevineKeySystem,
                          content::CdmInfo::Robustness::kSoftwareSecure,
                          std::move(capability),
                          /*supports_sub_key_systems=*/false,
                          kWidevineCdmDisplayName, kWidevineCdmType,
                          cdm_library_path);
}

void AddLibraryWidevineCdm(std::vector<content::CdmInfo>* cdms) {
  std::optional<content::CdmInfo> cdm_info =
      CreateLibraryWidevineCdmInfo(base::FilePath(kSystemCdmBasePath));
  if (cdm_info) {
    cdms->push_back(*std::move(cdm_info));
  }
}

}  // namespace neva_app_runtime
