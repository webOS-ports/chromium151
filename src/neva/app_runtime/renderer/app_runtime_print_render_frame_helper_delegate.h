// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_RUNTIME_RENDERER_APP_RUNTIME_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
#define NEVA_APP_RUNTIME_RENDERER_APP_RUNTIME_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_

#include "components/printing/renderer/print_render_frame_helper.h"

namespace neva_app_runtime {

// For the browser_shell's PageContents.PrintToPDF, which prints a page the
// way headless Chrome's Page.printToPDF does. The engine has no print dialog
// and no preview, so a page's own window.print() still does nothing.
class AppRuntimePrintRenderFrameHelperDelegate
    : public printing::PrintRenderFrameHelper::Delegate {
 public:
  AppRuntimePrintRenderFrameHelperDelegate();
  AppRuntimePrintRenderFrameHelperDelegate(
      const AppRuntimePrintRenderFrameHelperDelegate&) = delete;
  AppRuntimePrintRenderFrameHelperDelegate& operator=(
      const AppRuntimePrintRenderFrameHelperDelegate&) = delete;
  ~AppRuntimePrintRenderFrameHelperDelegate() override;

 private:
  // printing::PrintRenderFrameHelper::Delegate:
  blink::WebElement GetPdfElement(blink::WebLocalFrame* frame) override;
  bool IsPrintPreviewEnabled() override;
  bool IsScriptedPrintEnabled() override;
  bool OverridePrint(blink::WebLocalFrame* frame) override;
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_RENDERER_APP_RUNTIME_PRINT_RENDER_FRAME_HELPER_DELEGATE_H_
