// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCOPED_PAGE_PAUSER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCOPED_PAGE_PAUSER_H_

#include <memory>

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class ScopedBrowsingContextGroupPauser;
class ScopedPagePauser;
class WebLocalFrame;
class WebLocalFrameImpl;

// WebScopedPagePauser implements the concept of 'pause' in HTML standard.
// https://html.spec.whatwg.org/C/#pause
// All script execution is suspended while any of WebScopedPagePauser instances
// exists.
class WebScopedPagePauser {
 public:
// TODO(neva): It's needed in order to use WebScopedPagePauser in Neva's
// content::RenderThreadImpl::ProcessSuspend() and
// neva_app_runtime::AppRuntimeRenderFrameObserver::SuspendDOM().
// Bug: http://clm.lge.com/issue/browse/NEVA-8471
#if defined(USE_NEVA_APPRUNTIME)
  BLINK_EXPORT
#endif  // defined(USE_NEVA_APPRUNTIME)
  explicit WebScopedPagePauser(WebLocalFrameImpl&);

#if defined(USE_NEVA_APPRUNTIME)
  // The constructor above takes WebLocalFrameImpl, which lives under
  // //third_party/blink/renderer and may only be included from inside blink -
  // M151 made that a hard error, and the -blink mojom variants it pulls in
  // trip it. Both neva callers sit outside blink (content::RenderThreadImpl
  // and neva_app_runtime::AppRuntimeRenderFrameObserver), so they go through
  // this factory instead and the downcast happens on the blink side.
  BLINK_EXPORT static std::unique_ptr<WebScopedPagePauser> Create(
      WebLocalFrame&);
#endif  // defined(USE_NEVA_APPRUNTIME)

  WebScopedPagePauser(const WebScopedPagePauser&) = delete;
  WebScopedPagePauser& operator=(const WebScopedPagePauser&) = delete;
  BLINK_EXPORT ~WebScopedPagePauser();

 private:
  std::unique_ptr<ScopedPagePauser> page_pauser_;
  std::unique_ptr<ScopedBrowsingContextGroupPauser>
      browsing_context_group_pauser_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCOPED_PAGE_PAUSER_H_
