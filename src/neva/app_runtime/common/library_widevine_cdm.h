// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_RUNTIME_COMMON_LIBRARY_WIDEVINE_CDM_H_
#define NEVA_APP_RUNTIME_COMMON_LIBRARY_WIDEVINE_CDM_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "content/public/common/cdm_info.h"
#include "media/cdm/cdm_type.h"

namespace neva_app_runtime {

// Describes the stock Widevine library CDM unpacked under |cdm_base_path|:
//   <cdm_base_path>/manifest.json
//   <cdm_base_path>/_platform_specific/<os>_<arch>/libwidevinecdm.so
// Codecs, encryption schemes and session types come from the manifest, so they
// always describe the module that is really there. Returns nullopt if the
// library is absent or its manifest is unusable.
std::optional<content::CdmInfo> CreateLibraryWidevineCdmInfo(
    const base::FilePath& cdm_base_path);

// The Widevine bridge to the device's Android Widevine HAL
// (luneos-widevine-bridge): a second library CDM for the same key system,
// under its own type, so a site can get it instead of Google's.
inline constexpr media::CdmType kWidevineBridgeCdmType{0x4d49d3727f2832efull,
                                                       0xd7591b8763e729a1ull};

// Describes the bridge unpacked under |cdm_base_path| in the same layout as
// above, plus <cdm_base_path>/sites, the sites that get it (see
// ParseWidevineBridgeSites()). Returns nullopt if the library or manifest is
// absent or unusable, or no site is listed: the bridge is then not offered.
std::optional<content::CdmInfo> CreateWidevineBridgeCdmInfo(
    const base::FilePath& cdm_base_path,
    std::vector<std::string>* sites);

// One domain per line, matched with its subdomains; '#' starts a comment.
// Domains are lowercased, and a leading "." or "*." is ignored.
std::vector<std::string> ParseWidevineBridgeSites(std::string_view text);

// Whether |host| is one of |sites| or a subdomain of one.
bool HostMatchesWidevineBridgeSites(std::string_view host,
                                    const std::vector<std::string>& sites);

// Registers the CDMs installed in the system locations: Google's first, so it
// stays the CDM for capabilities and for every site the bridge does not list,
// then the bridge. Neither module is part of the image, so their absence is the
// normal case and leaves |cdms| unchanged.
void AddLibraryWidevineCdm(std::vector<content::CdmInfo>* cdms);

// Which Widevine CDM a frame on |frame_host|, in a page on |top_host|, gets.
// nullopt until AddLibraryWidevineCdm() registered the bridge, so nothing
// changes without it. After that: the bridge if either host is listed, else
// Google's CDM, never the bridge by default.
std::optional<media::CdmType> WidevineCdmTypeFor(std::string_view frame_host,
                                                 std::string_view top_host);

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_COMMON_LIBRARY_WIDEVINE_CDM_H_
