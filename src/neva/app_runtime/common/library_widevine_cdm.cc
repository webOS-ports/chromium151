// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_runtime/common/library_widevine_cdm.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "components/cdm/common/cdm_manifest.h"
#include "media/base/cdm_capability.h"
#include "media/cdm/cdm_paths.h"  // nogncheck
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck

namespace neva_app_runtime {

namespace {

constexpr base::FilePath::CharType kSystemCdmBasePath[] =
    FILE_PATH_LITERAL("/var/lib/widevine/WidevineCdm");
constexpr base::FilePath::CharType kSystemBridgeCdmBasePath[] =
    FILE_PATH_LITERAL("/var/lib/widevine/BridgeCdm");
constexpr char kWidevineBridgeCdmDisplayName[] =
    "Widevine Content Decryption Module (LuneOS bridge)";
// The sites file is a short hand-edited list; anything bigger is a mistake.
constexpr size_t kMaxSitesFileSize = 64 * 1024;

// Set once at CDM registration, read when a frame creates a CDM.
struct BridgeSites {
  base::Lock lock;
  std::vector<std::string> sites;
};
BridgeSites& GetBridgeSites() {
  static base::NoDestructor<BridgeSites> bridge_sites;
  return *bridge_sites;
}

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

std::vector<std::string> ParseWidevineBridgeSites(std::string_view text) {
  std::vector<std::string> sites;
  for (std::string_view line : base::SplitStringPiece(
           text, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    line = line.substr(0, line.find('#'));
    line = base::TrimWhitespaceASCII(line, base::TRIM_ALL);
    if (base::StartsWith(line, "*.")) {
      line.remove_prefix(2);
    } else if (base::StartsWith(line, ".")) {
      line.remove_prefix(1);
    }
    if (line.empty() || line.find_first_of(" \t/:") != std::string_view::npos) {
      continue;
    }
    sites.push_back(base::ToLowerASCII(line));
  }
  return sites;
}

bool HostMatchesWidevineBridgeSites(std::string_view host,
                                    const std::vector<std::string>& sites) {
  if (host.empty()) {
    return false;
  }
  const std::string lower = base::ToLowerASCII(host);
  for (const std::string& site : sites) {
    if (lower == site) {
      return true;
    }
    // A subdomain: "www.example.com" for "example.com", never "badexample.com".
    if (lower.size() > site.size() &&
        base::EndsWith(lower, site) &&
        lower[lower.size() - site.size() - 1] == '.') {
      return true;
    }
  }
  return false;
}

std::optional<content::CdmInfo> CreateWidevineBridgeCdmInfo(
    const base::FilePath& cdm_base_path,
    std::vector<std::string>* sites) {
  std::optional<content::CdmInfo> cdm_info =
      CreateLibraryWidevineCdmInfo(cdm_base_path);
  if (!cdm_info) {
    return std::nullopt;
  }
  std::string text;
  if (!base::ReadFileToStringWithMaxSize(
          cdm_base_path.Append(FILE_PATH_LITERAL("sites")), &text,
          kMaxSitesFileSize)) {
    LOG(WARNING) << "Widevine bridge in " << cdm_base_path
                 << " has no readable sites file; not offering it";
    return std::nullopt;
  }
  std::vector<std::string> parsed = ParseWidevineBridgeSites(text);
  if (parsed.empty()) {
    return std::nullopt;
  }
  cdm_info->type = kWidevineBridgeCdmType;
  cdm_info->name = kWidevineBridgeCdmDisplayName;
  *sites = std::move(parsed);
  return cdm_info;
}

void AddLibraryWidevineCdm(std::vector<content::CdmInfo>* cdms) {
  std::optional<content::CdmInfo> cdm_info =
      CreateLibraryWidevineCdmInfo(base::FilePath(kSystemCdmBasePath));
  if (cdm_info) {
    cdms->push_back(*std::move(cdm_info));
  }

  std::vector<std::string> sites;
  std::optional<content::CdmInfo> bridge_info = CreateWidevineBridgeCdmInfo(
      base::FilePath(kSystemBridgeCdmBasePath), &sites);
  if (bridge_info) {
    VLOG(1) << "Widevine bridge offered to " << sites.size() << " site(s)";
    cdms->push_back(*std::move(bridge_info));
    BridgeSites& bridge_sites = GetBridgeSites();
    base::AutoLock lock(bridge_sites.lock);
    bridge_sites.sites = std::move(sites);
  }
}

std::optional<media::CdmType> WidevineCdmTypeFor(std::string_view frame_host,
                                                 std::string_view top_host) {
  BridgeSites& bridge_sites = GetBridgeSites();
  base::AutoLock lock(bridge_sites.lock);
  if (bridge_sites.sites.empty()) {
    return std::nullopt;
  }
  if (HostMatchesWidevineBridgeSites(frame_host, bridge_sites.sites) ||
      HostMatchesWidevineBridgeSites(top_host, bridge_sites.sites)) {
    return kWidevineBridgeCdmType;
  }
  return kWidevineCdmType;
}

}  // namespace neva_app_runtime
