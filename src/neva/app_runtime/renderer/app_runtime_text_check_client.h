// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_RUNTIME_RENDERER_APP_RUNTIME_TEXT_CHECK_CLIENT_H_
#define NEVA_APP_RUNTIME_RENDERER_APP_RUNTIME_TEXT_CHECK_CLIENT_H_

#include <memory>
#include <vector>

#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/web/web_text_check_client.h"

namespace neva_app_runtime {

// Says which words in an editable field are misspelled, so that Blink marks
// them the way webOS did - a dotted line under the word.
//
// webOS had that from SmartKeyService, an XT9 and Hunspell service its WebKit
// asked over the bus. Here the same job is done in the renderer with the
// system's own Hunspell and the dictionaries the keyboard already uses
// (/usr/share/hunspell/<language>.{aff,dic}), both found at run time: the
// library is opened with dlopen, so nothing is linked and a device without it
// simply gets no spell checking.
//
// Where a field is checked at all is Blink's business - the spellcheck
// attribute, x-palm-disable-ste-all, and never a password - and this only
// answers for the words it is handed.
class AppRuntimeTextCheckClient : public content::RenderFrameObserver,
                                  public blink::WebTextCheckClient {
 public:
  explicit AppRuntimeTextCheckClient(content::RenderFrame* render_frame);
  AppRuntimeTextCheckClient(const AppRuntimeTextCheckClient&) = delete;
  AppRuntimeTextCheckClient& operator=(const AppRuntimeTextCheckClient&) =
      delete;
  ~AppRuntimeTextCheckClient() override;

  // content::RenderFrameObserver:
  void OnDestruct() override;

  // blink::WebTextCheckClient:
  bool IsSpellCheckingEnabled() const override;
  void CheckSpelling(const blink::WebString& text,
                     size_t& misspelled_offset,
                     size_t& misspelled_length,
                     std::vector<blink::WebString>* optional_suggestions)
      override;
  void RequestCheckingOfText(
      const blink::WebString& text_to_check,
      const std::vector<blink::WebSpellingMarker>& spelling_markers,
      ShouldForceRefreshTextCheckService should_force_refresh,
      std::unique_ptr<blink::WebTextCheckingCompletion> completion) override;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_RENDERER_APP_RUNTIME_TEXT_CHECK_CLIENT_H_
