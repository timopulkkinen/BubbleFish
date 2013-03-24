// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COMPOSITOR_FRAME_ACK_H_
#define CC_OUTPUT_COMPOSITOR_FRAME_ACK_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/gl_frame_data.h"
#include "cc/resources/transferable_resource.h"
#include "ui/surface/transport_dib.h"

namespace cc {

class CC_EXPORT CompositorFrameAck {
 public:
  CompositorFrameAck();
  ~CompositorFrameAck();

  TransferableResourceArray resources;
  scoped_ptr<GLFrameData> gl_frame_data;
  TransportDIB::Handle last_content_dib;
};

}  // namespace cc

#endif  // CC_OUTPUT_COMPOSITOR_FRAME_ACK_H_
