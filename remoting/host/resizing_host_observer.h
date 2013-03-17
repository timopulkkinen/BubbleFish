// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_RESIZING_HOST_OBSERVER_H_
#define REMOTING_HOST_RESIZING_HOST_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/session_controller.h"
#include "third_party/skia/include/core/SkSize.h"

namespace remoting {

class DesktopResizer;

// TODO(alexeypa): Rename this class to reflect that it is not
// HostStatusObserver any more.

// Use the specified DesktopResizer to match host desktop size to the client
// view size as closely as is possible. When the connection closes, restore
// the original desktop size.
class ResizingHostObserver : public SessionController {
 public:
  explicit ResizingHostObserver(scoped_ptr<DesktopResizer> desktop_resizer);
  virtual ~ResizingHostObserver();

  // SessionController interface.
  virtual void OnClientResolutionChanged(const SkIPoint& client_dpi,
                                         const SkISize& client_size) OVERRIDE;

 private:
  scoped_ptr<DesktopResizer> desktop_resizer_;
  SkISize original_size_;

  DISALLOW_COPY_AND_ASSIGN(ResizingHostObserver);
};

}  // namespace remoting

#endif  // REMOTING_HOST_RESIZING_HOST_OBSERVER_H_
