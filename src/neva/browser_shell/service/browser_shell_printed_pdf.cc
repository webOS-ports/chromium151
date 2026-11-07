// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/browser_shell/service/browser_shell_printed_pdf.h"

#include <string_view>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/strings/string_number_conversions.h"

namespace browser_shell {

namespace {

constexpr int kDirMode = 0700;

// A name no file in |dir| has yet: the time in milliseconds, with a count
// after it for prints in the same millisecond.
base::FilePath NewName(const base::FilePath& dir, base::Time now) {
  const std::string stem =
      base::NumberToString(now.InMillisecondsSinceUnixEpoch());
  base::FilePath path = dir.Append(stem + ".pdf");
  for (int n = 1; base::PathExists(path); ++n) {
    path = dir.Append(stem + "-" + base::NumberToString(n) + ".pdf");
  }
  return path;
}

void RemoveOlder(const base::FilePath& dir,
                 base::Time now,
                 base::TimeDelta max_age) {
  base::FileEnumerator files(dir, /*recursive=*/false,
                             base::FileEnumerator::FILES,
                             FILE_PATH_LITERAL("*.pdf"));
  for (base::FilePath path = files.Next(); !path.empty();
       path = files.Next()) {
    if (now - files.GetInfo().GetLastModifiedTime() > max_age) {
      base::DeleteFile(path);
    }
  }
}

}  // namespace

base::FilePath WritePrintedPdf(const base::FilePath& dir,
                               base::span<const uint8_t> data,
                               base::Time now,
                               base::TimeDelta max_age,
                               std::string* error) {
  if (dir.empty() || !dir.IsAbsolute()) {
    *error = "No folder to write the PDF in";
    return base::FilePath();
  }
  if (data.empty()) {
    *error = "The page printed as an empty PDF";
    return base::FilePath();
  }

  // The folder is below the profile, which only the browser's user can open,
  // so nobody else can put a link in its place. It is still refused as a
  // link, so that a profile set up some other way does not send the PDFs,
  // and the removal of the old ones, somewhere else.
  if (base::IsLink(dir)) {
    *error = dir.value() + " is a link";
    return base::FilePath();
  }
  base::File::Error mkdir_error = base::File::FILE_OK;
  if (!base::CreateDirectoryAndGetError(dir, &mkdir_error)) {
    *error = "Could not make " + dir.value() + ": " +
             base::File::ErrorToString(mkdir_error);
    return base::FilePath();
  }
  if (!base::SetPosixFilePermissions(dir, kDirMode)) {
    *error = "Could not limit " + dir.value() + " to its owner";
    return base::FilePath();
  }

  RemoveOlder(dir, now, max_age);

  // ImportantFileWriter writes a temporary file (mkstemp, 0600) next to the
  // target and renames it, so a reader never sees half a PDF.
  const base::FilePath path = NewName(dir, now);
  const std::string_view bytes(reinterpret_cast<const char*>(data.data()),
                               data.size());
  if (!base::ImportantFileWriter::WriteFileAtomically(path, bytes)) {
    *error = "Could not write " + path.value();
    return base::FilePath();
  }
  return path;
}

}  // namespace browser_shell
