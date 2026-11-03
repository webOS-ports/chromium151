// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_runtime/common/library_widevine_cdm.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/native_library.h"
#include "media/base/content_decryption_module.h"
#include "media/base/encryption_scheme.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_paths.h"  // nogncheck
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck

namespace neva_app_runtime {

namespace {

// The manifest of Widevine CDM 4.10.3112.0 as Google ships it for Linux,
// reduced to the fields the parser reads.
constexpr char kManifest[] = R"({
  "version": "4.10.3112.0",
  "x-cdm-module-versions": "4",
  "x-cdm-interface-versions": "10",
  "x-cdm-host-versions": "10,11",
  "x-cdm-codecs": "vp8,vp09,avc1,av01",
  "x-cdm-persistent-license-support": false,
  "x-cdm-supported-encryption-schemes": ["cenc", "cbcs"]
})";

class LibraryWidevineCdmTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(dir_.CreateUniqueTempDir()); }

  base::FilePath library_path() const {
    return media::GetPlatformSpecificDirectory(dir_.GetPath())
        .Append(base::GetNativeLibraryName(kWidevineCdmLibraryName));
  }

  void WriteLibrary() {
    ASSERT_TRUE(base::CreateDirectory(library_path().DirName()));
    ASSERT_TRUE(base::WriteFile(library_path(), ""));
  }

  void WriteManifest(const std::string& manifest) {
    ASSERT_TRUE(base::WriteFile(
        dir_.GetPath().Append(FILE_PATH_LITERAL("manifest.json")), manifest));
  }

  base::ScopedTempDir dir_;
};

TEST_F(LibraryWidevineCdmTest, NothingInstalled) {
  EXPECT_FALSE(CreateLibraryWidevineCdmInfo(dir_.GetPath()));
}

TEST_F(LibraryWidevineCdmTest, ManifestWithoutLibrary) {
  WriteManifest(kManifest);
  EXPECT_FALSE(CreateLibraryWidevineCdmInfo(dir_.GetPath()));
}

TEST_F(LibraryWidevineCdmTest, LibraryWithoutManifest) {
  WriteLibrary();
  EXPECT_FALSE(CreateLibraryWidevineCdmInfo(dir_.GetPath()));
}

TEST_F(LibraryWidevineCdmTest, UnparsableManifest) {
  WriteLibrary();
  WriteManifest("{ not json");
  EXPECT_FALSE(CreateLibraryWidevineCdmInfo(dir_.GetPath()));
}

// A CDM speaking only an interface this host cannot drive must not be offered:
// the page would be told Widevine works and then fail at playback.
TEST_F(LibraryWidevineCdmTest, IncompatibleInterfaceVersion) {
  WriteLibrary();
  std::string manifest(kManifest);
  const std::string from = "\"x-cdm-interface-versions\": \"10\"";
  manifest.replace(manifest.find(from), from.size(),
                   "\"x-cdm-interface-versions\": \"9999\"");
  WriteManifest(manifest);
  EXPECT_FALSE(CreateLibraryWidevineCdmInfo(dir_.GetPath()));
}

TEST_F(LibraryWidevineCdmTest, DescribesTheInstalledModule) {
  WriteLibrary();
  WriteManifest(kManifest);

  const std::optional<content::CdmInfo> info =
      CreateLibraryWidevineCdmInfo(dir_.GetPath());
  ASSERT_TRUE(info);

  EXPECT_EQ(kWidevineKeySystem, info->key_system);
  EXPECT_EQ(kWidevineCdmType, info->type);
  EXPECT_EQ(library_path(), info->path);
  EXPECT_FALSE(info->supports_sub_key_systems);

  // Claiming more than software security would promise pages a protected path
  // that does not exist.
  EXPECT_EQ(content::CdmInfo::Robustness::kSoftwareSecure, info->robustness);

  ASSERT_TRUE(info->capability);
  const media::CdmCapability& capability = *info->capability;
  EXPECT_EQ(base::Version("4.10.3112.0"), capability.version);
  EXPECT_TRUE(capability.video_codecs.contains(media::VideoCodec::kVP8));
  EXPECT_TRUE(capability.video_codecs.contains(media::VideoCodec::kVP9));
  EXPECT_TRUE(capability.video_codecs.contains(media::VideoCodec::kH264));
  EXPECT_TRUE(capability.video_codecs.contains(media::VideoCodec::kAV1));
  EXPECT_FALSE(capability.video_codecs.contains(media::VideoCodec::kHEVC));
  EXPECT_TRUE(
      capability.encryption_schemes.contains(media::EncryptionScheme::kCenc));
  EXPECT_TRUE(
      capability.encryption_schemes.contains(media::EncryptionScheme::kCbcs));
  EXPECT_TRUE(
      capability.session_types.contains(media::CdmSessionType::kTemporary));
  EXPECT_FALSE(capability.session_types.contains(
      media::CdmSessionType::kPersistentLicense));
}

TEST_F(LibraryWidevineCdmTest, BridgeNeedsSitesFile) {
  WriteLibrary();
  WriteManifest(kManifest);
  std::vector<std::string> sites;
  EXPECT_FALSE(CreateWidevineBridgeCdmInfo(dir_.GetPath(), &sites));
  EXPECT_TRUE(sites.empty());
}

TEST_F(LibraryWidevineCdmTest, BridgeWithOnlyCommentsIsNotOffered) {
  WriteLibrary();
  WriteManifest(kManifest);
  ASSERT_TRUE(base::WriteFile(dir_.GetPath().Append(FILE_PATH_LITERAL("sites")),
                              "# nothing yet\n\n"));
  std::vector<std::string> sites;
  EXPECT_FALSE(CreateWidevineBridgeCdmInfo(dir_.GetPath(), &sites));
}

TEST_F(LibraryWidevineCdmTest, DescribesTheBridge) {
  WriteLibrary();
  WriteManifest(kManifest);
  ASSERT_TRUE(base::WriteFile(dir_.GetPath().Append(FILE_PATH_LITERAL("sites")),
                              "npo.nl\nprimevideo.com  # Prime\n"));
  std::vector<std::string> sites;
  const std::optional<content::CdmInfo> info =
      CreateWidevineBridgeCdmInfo(dir_.GetPath(), &sites);
  ASSERT_TRUE(info);
  // Its own type, so it runs in its own CDM service, apart from Google's.
  EXPECT_EQ(kWidevineBridgeCdmType, info->type);
  EXPECT_NE(kWidevineCdmType, info->type);
  EXPECT_EQ(kWidevineKeySystem, info->key_system);
  EXPECT_EQ(library_path(), info->path);
  EXPECT_EQ(content::CdmInfo::Robustness::kSoftwareSecure, info->robustness);
  EXPECT_EQ((std::vector<std::string>{"npo.nl", "primevideo.com"}), sites);
}

TEST(WidevineBridgeSitesTest, Parse) {
  EXPECT_EQ((std::vector<std::string>{"npo.nl", "primevideo.com", "amazon.de",
                                      "example.org"}),
            ParseWidevineBridgeSites("  NPO.nl\r\n"
                                     "# a comment\n"
                                     "primevideo.com # trailing comment\n"
                                     "*.amazon.de\n"
                                     ".example.org\n"
                                     "\n"
                                     "https://bad.example/\n"
                                     "two words.example\n"));
  EXPECT_TRUE(ParseWidevineBridgeSites("").empty());
  EXPECT_TRUE(ParseWidevineBridgeSites("#\n# only comments\n").empty());
}

TEST(WidevineBridgeSitesTest, Match) {
  const std::vector<std::string> sites = {"npo.nl", "primevideo.com"};
  EXPECT_TRUE(HostMatchesWidevineBridgeSites("npo.nl", sites));
  EXPECT_TRUE(HostMatchesWidevineBridgeSites("www.npo.nl", sites));
  EXPECT_TRUE(HostMatchesWidevineBridgeSites("start.npo.nl", sites));
  EXPECT_TRUE(HostMatchesWidevineBridgeSites("WWW.PrimeVideo.com", sites));
  // Not a subdomain, only a name that ends the same way.
  EXPECT_FALSE(HostMatchesWidevineBridgeSites("notnpo.nl", sites));
  EXPECT_FALSE(HostMatchesWidevineBridgeSites("npo.nl.evil.example", sites));
  EXPECT_FALSE(HostMatchesWidevineBridgeSites("www.netflix.com", sites));
  EXPECT_FALSE(HostMatchesWidevineBridgeSites("nl", sites));
  // An opaque origin has no host.
  EXPECT_FALSE(HostMatchesWidevineBridgeSites("", sites));
  EXPECT_FALSE(HostMatchesWidevineBridgeSites("npo.nl", {}));
}

}  // namespace

}  // namespace neva_app_runtime
