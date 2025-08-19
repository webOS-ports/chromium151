// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {UrlDataView, UrlTypeMapper} from './url.mojom-converters.js';

export class UrlConverter implements UrlTypeMapper<string> {
  url(url: string): string {
    return url;
  }

  // NEVA: url.mojom's Url carries an optional webapp_id under
  // use_neva_appruntime, so the generated UrlTypeMapper requires this method.
  // The TypeScript mapping represents a URL as a plain string, which has
  // nowhere to hold a webOS application id, so there is never one to report.
  webappId(_url: string): (string|null) {
    return null;
  }

  convert(view: UrlDataView): string {
    return view.url;
  }
}
