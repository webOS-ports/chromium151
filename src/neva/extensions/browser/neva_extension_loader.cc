// Copyright 2022 LG Electronics, Inc.
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

#include "neva/extensions/browser/neva_extension_loader.h"

#include "base/command_line.h"
#include "base/auto_reset.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/constants.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/file_util.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"

namespace neva {

namespace {

scoped_refptr<const extensions::Extension> LoadUnpacked(
    const base::FilePath& extension_dir) {
  // app_shell only supports unpacked extensions.
  // NOTE: If you add packed extension support consider removing the flag
  // FOLLOW_SYMLINKS_ANYWHERE below. Packed extensions should not have symlinks.
  if (!base::DirectoryExists(extension_dir)) {
    LOG(ERROR) << "Extension directory not found: "
               << extension_dir.AsUTF8Unsafe();
    return nullptr;
  }

  int load_flags = extensions::Extension::FOLLOW_SYMLINKS_ANYWHERE;
  std::u16string load_error;
  scoped_refptr<extensions::Extension> extension =
      extensions::file_util::LoadExtension(
          extension_dir, extensions::mojom::ManifestLocation::kCommandLine,
          load_flags, &load_error);
  if (!extension.get()) {
    LOG(ERROR) << "Loading extension at " << extension_dir.value()
               << " failed with: " << base::UTF16ToUTF8(load_error);
    return nullptr;
  }

  // Log warnings.
  if (extension->install_warnings().size()) {
    LOG(WARNING) << "Warnings loading extension at " << extension_dir.value()
                 << ":";
    for (const auto& warning : extension->install_warnings())
      LOG(WARNING) << warning.message;
  }

  return extension;
}

}  // namespace

NevaExtensionLoader::NevaExtensionLoader(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      extension_registrar_(
          extensions::ExtensionRegistrar::Get(browser_context)) {
  // M151: ExtensionRegistrar became a KeyedService that the embedder has to
  // initialise; without this its delegate_ is null and AddExtension() aborts
  // on CHECK(delegate_). Chrome does this from ExtensionService's ctor.
  if (!extension_registrar_->IsInitialized()) {
    extension_registrar_->Init(
        this, /*extensions_enabled=*/true, base::CommandLine::ForCurrentProcess(),
        browser_context->GetPath().AppendASCII(extensions::kInstallDirectoryName),
        browser_context->GetPath().AppendASCII(
            extensions::kUnpackedInstallDirectoryName));
  }
}

NevaExtensionLoader::~NevaExtensionLoader() = default;

const extensions::Extension* NevaExtensionLoader::LoadExtension(
    const base::FilePath& extension_dir) {
  scoped_refptr<const extensions::Extension> extension =
      LoadUnpacked(extension_dir);
  if (extension) {
    // Provide (over)permission for all loaded extensions to pass
    // PermissionsData::CanAccessPage(). In chrome browser, such action is done
    // via ActiveTabPermissionGranter. But we don't have such mechanism yet.
    // TODO(neva): Remove this once we make alternative way.
    std::unique_ptr<const extensions::PermissionSet> withheld =
        extension->permissions_data()->withheld_permissions().Clone();
    std::unique_ptr<const extensions::PermissionSet> active =
        extension->permissions_data()->active_permissions().Clone();

    extensions::URLPatternSet allowed_url_patterns;
    allowed_url_patterns.AddPattern(
        URLPattern(URLPattern::SchemeMasks::SCHEME_HTTP, "http://*/*"));
    allowed_url_patterns.AddPattern(
        URLPattern(URLPattern::SchemeMasks::SCHEME_HTTPS, "https://*/*"));
    extensions::PermissionSet new_permission_set(
        {}, {}, std::move(allowed_url_patterns), {});

    std::unique_ptr<const extensions::PermissionSet> new_active =
        extensions::PermissionSet::CreateUnion(*active, new_permission_set);

    extension->permissions_data()->SetPermissions(std::move(new_active),
                                                  std::move(withheld));

    extension_registrar_->AddExtension(extension);
  }

  return extension.get();
}

void NevaExtensionLoader::ReloadExtension(
    extensions::ExtensionId extension_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context_)
          ->GetInstalledExtension(extension_id);
  // We shouldn't be trying to reload extensions that haven't been added.
  DCHECK(extension);

  // This should always start false since it's only set here, or in
  // LoadExtensionForReload() as a result of the call below.
  DCHECK_EQ(false, did_schedule_reload_);
  base::AutoReset<bool> reset_did_schedule_reload(&did_schedule_reload_, false);

  extension_registrar_->ReloadExtensionWithQuietFailure(extension_id);
  // if (did_schedule_reload_)
  //   return;
}

void NevaExtensionLoader::FinishExtensionReload(
    const extensions::ExtensionId old_extension_id,
    scoped_refptr<const extensions::Extension> extension) {
  if (extension) {
    extension_registrar_->AddExtension(std::move(extension));
  }
}

void NevaExtensionLoader::PreAddExtension(
    const extensions::Extension* extension,
    const extensions::Extension* old_extension) {
  if (old_extension)
    return;

  // The extension might be disabled if a previous reload attempt failed. In
  // that case, we want to remove that disable reason.
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser_context_);
  if (extension_prefs->IsExtensionDisabled(extension->id()) &&
      extension_prefs->HasDisableReason(
          extension->id(), extensions::disable_reason::DISABLE_RELOAD)) {
    // M151: the registrar does remove-reason-and-re-enable-if-clear in one
    // step; ExtensionPrefs::SetExtensionEnabled is gone and GetDisableReasons
    // now returns a DisableReasonSet rather than a bitmask.
    extension_registrar_->RemoveDisableReasonAndMaybeEnable(
        extension->id(), extensions::disable_reason::DISABLE_RELOAD);
  }
}

void NevaExtensionLoader::PostActivateExtension(
    scoped_refptr<const extensions::Extension> extension) {}

void NevaExtensionLoader::PostDeactivateExtension(
    scoped_refptr<const extensions::Extension> extension) {}

void NevaExtensionLoader::LoadExtensionForReload(
    const extensions::ExtensionId& extension_id,
    const base::FilePath& path) {
  CHECK(!path.empty());

  extensions::GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadUnpacked, path),
      base::BindOnce(&NevaExtensionLoader::FinishExtensionReload,
                     weak_factory_.GetWeakPtr(), extension_id));
  did_schedule_reload_ = true;
}

// M151: the quiet-failure variant exists so the registrar can reload without
// surfacing an error to the user. Nothing here surfaces load errors anyway.
void NevaExtensionLoader::LoadExtensionForReloadWithQuietFailure(
    const extensions::ExtensionId& extension_id,
    const base::FilePath& path) {
  LoadExtensionForReload(extension_id, path);
}

bool NevaExtensionLoader::CanEnableExtension(const extensions::Extension* extension) {
  return true;
}

bool NevaExtensionLoader::CanDisableExtension(const extensions::Extension* extension) {
  // Extensions cannot be disabled by the user.
  return false;
}

// The remaining Delegate hooks below became pure virtual in M151. They belong
// to the packed-install and enterprise-policy paths that this loader does not
// have: extensions are loaded unpacked from the command line, never installed
// through CrxInstaller, uninstalled, or disabled by policy. ExtensionRegistrar
// only reaches them from paths this embedder never enters.
void NevaExtensionLoader::OnAddNewOrUpdatedExtension(const extensions::Extension* extension) {}

void NevaExtensionLoader::PreUninstallExtension(
    scoped_refptr<const extensions::Extension> extension) {}

void NevaExtensionLoader::PostUninstallExtension(
    scoped_refptr<const extensions::Extension> extension,
    base::OnceClosure done_callback) {
  std::move(done_callback).Run();
}

void NevaExtensionLoader::ShowExtensionDisabledError(const extensions::Extension* extension,
                                       bool is_remote_install) {}

void NevaExtensionLoader::GrantActivePermissions(const extensions::Extension* extension) {}

void NevaExtensionLoader::UpdateExternalExtensionAlert() {}

void NevaExtensionLoader::OnExtensionInstalled(const extensions::Extension* extension,
                                 const syncer::StringOrdinal& page_ordinal,
                                 int install_flags,
                                 base::DictValue ruleset_install_prefs) {}

}  // namespace neva
