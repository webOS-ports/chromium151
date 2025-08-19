// Copyright 2018 LG Electronics, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_NEVA_HTML_MEDIA_ELEMENT_STUB_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_NEVA_HTML_MEDIA_ELEMENT_STUB_H_

#include <stdio.h>

#include <limits>
#include <type_traits>


namespace blink {
namespace neva {

template <typename original_t>
class HTMLMediaElement {
 public:
  HTMLMediaElement();

  ScriptValue getStartDate(ScriptState* script_state) const;
  const String mediaId() const;

  // Neva audio focus extensions
  bool webosMediaFocus() const;
  void setWebosMediaFocus(bool focus);

  bool send(const String& message);
};

template <typename original_t>
HTMLMediaElement<original_t>::HTMLMediaElement() {}

template <typename original_t>
ScriptValue HTMLMediaElement<original_t>::getStartDate(
    ScriptState* script_state) const {
  // getStartDate() returns a Date instance.
  // M151 removed platform/bindings/to_v8.h. Build the Date directly, as
  // upstream now does in v8_binding_for_core.h.
  return ScriptValue(
      script_state->GetIsolate(),
      v8::Date::New(script_state->GetContext(),
                    std::numeric_limits<double>::quiet_NaN())
          .ToLocalChecked());
}

template <typename original_t>
const String HTMLMediaElement<original_t>::mediaId() const {
  return String();
}

template <typename original_t>
bool HTMLMediaElement<original_t>::webosMediaFocus() const {
  return false;
}

template <typename original_t>
void HTMLMediaElement<original_t>::setWebosMediaFocus(bool focus) {}

template <typename original_t>
bool HTMLMediaElement<original_t>::send(const String& message) {
  return false;
}

}  // namespace neva
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_NEVA_HTML_MEDIA_ELEMENT_STUB_H_
