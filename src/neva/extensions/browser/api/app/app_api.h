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

#ifndef NEVA_EXTENSIONS_BROWSER_API_APP_APP_API_H_
#define NEVA_EXTENSIONS_BROWSER_API_APP_APP_API_H_

// The chrome.app namespace declared in neva/extensions/common/api/app.json is
// a stub with no functions - it exists only so that `isChrome` style checks in
// extension JS find the namespace present. It therefore has no implementation.
//
// This header exists because M151's json_schema_compiler requires one per
// namespace: cpp_bundle_generator.py emits an unconditional #include of either
// the schema's compiler_options["implemented_in"] or the default
// {impl_dir}/{namespace}/{namespace}_api.h, and raises if the file is absent.
// M120's generator did not check, which is why the 120 tree builds without it.
//
// If chrome.app ever gains real functions, declare them here and the bundle
// will pick them up with no further build changes.

#endif  // NEVA_EXTENSIONS_BROWSER_API_APP_APP_API_H_
