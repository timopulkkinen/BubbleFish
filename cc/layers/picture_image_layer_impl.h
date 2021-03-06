// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_IMAGE_LAYER_IMPL_H_
#define CC_LAYERS_PICTURE_IMAGE_LAYER_IMPL_H_

#include "cc/layers/picture_layer_impl.h"

namespace cc {

class CC_EXPORT PictureImageLayerImpl : public PictureLayerImpl {
 public:
  static scoped_ptr<PictureImageLayerImpl> Create(LayerTreeImpl* treeImpl,
                                                  int id) {
    return make_scoped_ptr(new PictureImageLayerImpl(treeImpl, id));
  }
  virtual ~PictureImageLayerImpl();

  virtual const char* LayerTypeAsString() const OVERRIDE;
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* treeImpl) OVERRIDE;

 protected:
  PictureImageLayerImpl(LayerTreeImpl* treeImpl, int id);

  virtual void CalculateRasterContentsScale(
      bool animating_transform_to_screen,
      float* raster_contents_scale,
      float* low_res_raster_contents_scale) OVERRIDE;
  virtual void GetDebugBorderProperties(
      SkColor* color, float* width) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PictureImageLayerImpl);
};

}

#endif  // CC_LAYERS_PICTURE_IMAGE_LAYER_IMPL_H_
