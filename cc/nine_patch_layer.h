// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_NINE_PATCH_LAYER_H_
#define CC_NINE_PATCH_LAYER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/layer.h"
#include "cc/image_layer_updater.h"
#include "ui/gfx/rect.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

class ResourceUpdateQueue;

class CC_EXPORT NinePatchLayer : public Layer {
 public:
  static scoped_refptr<NinePatchLayer> Create();

  virtual bool DrawsContent() const OVERRIDE;
  virtual void SetTexturePriorities(const PriorityCalculator& priority_calc)
      OVERRIDE;
  virtual void Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker* occlusion,
                      RenderingStats* stats) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  // aperture is in the pixel space of the bitmap resource and refers to
  // the center patch of the ninepatch (which is unused in this
  // implementation). We split off eight rects surrounding it and stick them
  // on the edges of the layer. The corners are unscaled, the top and bottom
  // rects are x-stretched to fit, and the left and right rects are
  // y-stretched to fit.
  void SetBitmap(const SkBitmap& bitmap, gfx::Rect aperture);

 private:
  NinePatchLayer();
  virtual ~NinePatchLayer();
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  void CreateUpdaterIfNeeded();
  void CreateResource();

  scoped_refptr<ImageLayerUpdater> updater_;
  scoped_ptr<LayerUpdater::Resource> resource_;

  SkBitmap bitmap_;
  bool bitmap_dirty_;

  // The transparent center region that shows the parent layer's contents in
  // image space.
  gfx::Rect image_aperture_;
};

}  // namespace cc

#endif  // CC_NINE_PATCH_LAYER_H_
