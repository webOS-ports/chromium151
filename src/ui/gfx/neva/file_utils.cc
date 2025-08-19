// Copyright 2021 LG Electronics, Inc.
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

#include <optional>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/neva/file_utils.h"

namespace gfx {

SkBitmap* DecodeSkBitmapFromPNG(const base::FilePath& path) {
  if (path.empty())
    return nullptr;

  // M151: base::File::ReadAtCurrentPos() is private and
  // RefCountedBytes::TakeVector() is gone; ReadFileToBytes() covers both.
  std::optional<std::vector<uint8_t>> data = base::ReadFileToBytes(path);
  if (!data || data->empty()) {
    LOG(ERROR) << "Unable to read file path = " << path;
    return nullptr;
  }

  // M151: PNGCodec::Decode() takes a span and returns the bitmap by value.
  SkBitmap bitmap = gfx::PNGCodec::Decode(*data);
  if (bitmap.isNull()) {
    LOG(ERROR) << "Unable to decode image path = " << path;
    return nullptr;
  }

  return new SkBitmap(std::move(bitmap));
}

}  // namespace gfx
