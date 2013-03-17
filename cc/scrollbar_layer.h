// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCROLLBAR_LAYER_H_
#define CC_SCROLLBAR_LAYER_H_

#include "cc/cc_export.h"
#include "cc/contents_scaling_layer.h"
#include "cc/layer_updater.h"
#include "cc/scrollbar_theme_painter.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbar.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScrollbarThemeGeometry.h"

namespace cc {
class CachingBitmapContentLayerUpdater;
class ResourceUpdateQueue;
class Scrollbar;
class ScrollbarThemeComposite;

class CC_EXPORT ScrollbarLayer : public ContentsScalingLayer {
 public:
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  static scoped_refptr<ScrollbarLayer> Create(
      scoped_ptr<WebKit::WebScrollbar>,
      scoped_ptr<ScrollbarThemePainter>,
      scoped_ptr<WebKit::WebScrollbarThemeGeometry>,
      int scrollLayerId);

  int scroll_layer_id() const { return scroll_layer_id_; }
  void SetScrollLayerId(int id);

  virtual bool OpacityCanAnimateOnImplThread() const OVERRIDE;

  WebKit::WebScrollbar::Orientation Orientation() const;

  // Layer interface
  virtual void SetTexturePriorities(const PriorityCalculator& priority_calc)
      OVERRIDE;
  virtual void Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker* occlusion,
                      RenderingStats* stats) OVERRIDE;
  virtual void SetLayerTreeHost(LayerTreeHost* host) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* contentBounds) OVERRIDE;

  virtual ScrollbarLayer* ToScrollbarLayer() OVERRIDE;

 protected:
  ScrollbarLayer(
      scoped_ptr<WebKit::WebScrollbar>,
      scoped_ptr<ScrollbarThemePainter>,
      scoped_ptr<WebKit::WebScrollbarThemeGeometry>,
      int scrollLayerId);
  virtual ~ScrollbarLayer();

 private:
  void UpdatePart(CachingBitmapContentLayerUpdater* painter,
                  LayerUpdater::Resource* resource,
                  gfx::Rect rect,
                  ResourceUpdateQueue* queue,
                  RenderingStats* stats);
  void CreateUpdaterIfNeeded();
  gfx::Rect ScrollbarLayerRectToContentRect(gfx::Rect layer_rect) const;

  bool is_dirty() const { return !dirty_rect_.IsEmpty(); }

  int MaxTextureSize();
  float ClampScaleToMaxTextureSize(float scale);

  scoped_ptr<WebKit::WebScrollbar> scrollbar_;
  scoped_ptr<ScrollbarThemePainter> painter_;
  scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry_;
  gfx::Size thumb_size_;
  int scroll_layer_id_;

  unsigned texture_format_;

  gfx::RectF dirty_rect_;

  scoped_refptr<CachingBitmapContentLayerUpdater> back_track_updater_;
  scoped_refptr<CachingBitmapContentLayerUpdater> fore_track_updater_;
  scoped_refptr<CachingBitmapContentLayerUpdater> thumb_updater_;

  // All the parts of the scrollbar except the thumb
  scoped_ptr<LayerUpdater::Resource> back_track_;
  scoped_ptr<LayerUpdater::Resource> fore_track_;
  scoped_ptr<LayerUpdater::Resource> thumb_;
};

}  // namespace cc

#endif  // CC_SCROLLBAR_LAYER_H_
