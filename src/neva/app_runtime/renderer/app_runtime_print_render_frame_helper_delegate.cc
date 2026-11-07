// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_runtime/renderer/app_runtime_print_render_frame_helper_delegate.h"

#include "third_party/blink/public/web/web_element.h"

namespace neva_app_runtime {

AppRuntimePrintRenderFrameHelperDelegate::
    AppRuntimePrintRenderFrameHelperDelegate() = default;

AppRuntimePrintRenderFrameHelperDelegate::
    ~AppRuntimePrintRenderFrameHelperDelegate() = default;

blink::WebElement AppRuntimePrintRenderFrameHelperDelegate::GetPdfElement(
    blink::WebLocalFrame* frame) {
  return blink::WebElement();
}

bool AppRuntimePrintRenderFrameHelperDelegate::IsPrintPreviewEnabled() {
  return false;
}

// window.print() would ask the browser for print settings through
// PrintManagerHost, which this engine does not serve.
bool AppRuntimePrintRenderFrameHelperDelegate::IsScriptedPrintEnabled() {
  return false;
}

bool AppRuntimePrintRenderFrameHelperDelegate::OverridePrint(
    blink::WebLocalFrame* frame) {
  return false;
}

}  // namespace neva_app_runtime
