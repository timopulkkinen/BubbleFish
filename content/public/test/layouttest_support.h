// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_

#include "base/callback_forward.h"

namespace WebKit {
class WebGamepads;
}

namespace WebTestRunner {
class WebTestProxyBase;
}

namespace content {

class RenderView;

// Enable injecting of a WebTestProxy between WebViews and RenderViews.
// |callback| is invoked with a pointer to WebTestProxyBase for each created
// WebTestProxy.
void EnableWebTestProxyCreation(const base::Callback<
    void(RenderView*, WebTestRunner::WebTestProxyBase*)>& callback);

// Sets the WebGamepads that should be returned by
// WebKitPlatformSupport::sampleGamepads().
void SetMockGamepads(const WebKit::WebGamepads& pads);

// Disable logging to the console from the appcache system.
void DisableAppCacheLogging();

// Enable testing support in the devtools client.
void EnableDevToolsFrontendTesting();

// Returns the length of the local session history of a render view.
int GetLocalSessionHistoryLength(RenderView* render_view);

void SetAllowOSMesaImageTransportForTesting();

// Do not require a user gesture for focus change events.
void DoNotRequireUserGestureForFocusChanges();

// Sync the current session history to the browser process.
void SyncNavigationState(RenderView* render_view);

// Sets the focus of the render view depending on |enable|. This only overrides
// the state of the renderer, and does not sync the focus to the browser
// process.
void SetFocusAndActivate(RenderView* render_view, bool enable);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_LAYOUTTEST_SUPPORT_H_
