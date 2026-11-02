// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_RUNTIME_COMMON_LIBRARY_WIDEVINE_CDM_H_
#define NEVA_APP_RUNTIME_COMMON_LIBRARY_WIDEVINE_CDM_H_

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "content/public/common/cdm_info.h"

namespace neva_app_runtime {

// Describes the stock Widevine library CDM unpacked under |cdm_base_path|:
//   <cdm_base_path>/manifest.json
//   <cdm_base_path>/_platform_specific/<os>_<arch>/libwidevinecdm.so
// Codecs, encryption schemes and session types come from the manifest, so they
// always describe the module that is really there. Returns nullopt if the
// library is absent or its manifest is unusable.
std::optional<content::CdmInfo> CreateLibraryWidevineCdmInfo(
    const base::FilePath& cdm_base_path);

// Registers the CDM installed in the system location, if there is one. The
// module is not part of the image, so its absence is the normal case and leaves
// |cdms| unchanged.
void AddLibraryWidevineCdm(std::vector<content::CdmInfo>* cdms);

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_COMMON_LIBRARY_WIDEVINE_CDM_H_
