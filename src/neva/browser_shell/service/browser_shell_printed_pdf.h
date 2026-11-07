// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_BROWSER_SHELL_SERVICE_BROWSER_SHELL_PRINTED_PDF_H_
#define NEVA_BROWSER_SHELL_SERVICE_BROWSER_SHELL_PRINTED_PDF_H_

#include <cstdint>
#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/time/time.h"

namespace browser_shell {

// How long a printed PDF is kept. The print service reads the file when it
// sends the job, seconds after it was written; the next print removes the
// ones older than this.
inline constexpr base::TimeDelta kPrintedPdfMaxAge = base::Hours(1);

// Writes |data| as a new file in |dir|, which is made (0700) if it is
// missing, and removes the PDFs there that are older than |max_age| at
// |now|. Each call writes a file of its own, so a print that is still
// queued keeps its file while another is made. The file is written to a
// temporary name and renamed, and is readable by its owner only.
//
// Returns the file's path, or an empty path with |error| set.
base::FilePath WritePrintedPdf(const base::FilePath& dir,
                               base::span<const uint8_t> data,
                               base::Time now,
                               base::TimeDelta max_age,
                               std::string* error);

}  // namespace browser_shell

#endif  // NEVA_BROWSER_SHELL_SERVICE_BROWSER_SHELL_PRINTED_PDF_H_
