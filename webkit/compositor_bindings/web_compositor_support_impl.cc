// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_compositor_support_impl.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "cc/output_surface.h"
#include "cc/software_output_device.h"
#include "cc/thread_impl.h"
#include "cc/transform_operations.h"
#include "webkit/compositor_bindings/web_animation_impl.h"
#include "webkit/compositor_bindings/web_content_layer_impl.h"
#include "webkit/compositor_bindings/web_external_texture_layer_impl.h"
#include "webkit/compositor_bindings/web_float_animation_curve_impl.h"
#include "webkit/compositor_bindings/web_image_layer_impl.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_scrollbar_layer_impl.h"
#include "webkit/compositor_bindings/web_solid_color_layer_impl.h"
#include "webkit/compositor_bindings/web_transform_animation_curve_impl.h"
#include "webkit/compositor_bindings/web_transform_operations_impl.h"
#include "webkit/compositor_bindings/web_video_layer_impl.h"
#include "webkit/glue/webthread_impl.h"
#include "webkit/support/webkit_support.h"

using WebKit::WebAnimation;
using WebKit::WebAnimationCurve;
using WebKit::WebContentLayer;
using WebKit::WebContentLayerClient;
using WebKit::WebExternalTextureLayer;
using WebKit::WebExternalTextureLayerClient;
using WebKit::WebFloatAnimationCurve;
using WebKit::WebImageLayer;
using WebKit::WebImageLayer;
using WebKit::WebLayer;
using WebKit::WebScrollbar;
using WebKit::WebScrollbarLayer;
using WebKit::WebScrollbarThemeGeometry;
using WebKit::WebScrollbarThemePainter;
using WebKit::WebSolidColorLayer;
using WebKit::WebTransformAnimationCurve;
using WebKit::WebTransformOperations;
using WebKit::WebVideoFrameProvider;
using WebKit::WebVideoLayer;

namespace webkit {

WebCompositorSupportImpl::WebCompositorSupportImpl() : initialized_(false) {}

WebCompositorSupportImpl::~WebCompositorSupportImpl() {}

void WebCompositorSupportImpl::initialize(
    WebKit::WebThread* compositor_thread) {
  DCHECK(!initialized_);
  initialized_ = true;
  if (compositor_thread) {
    webkit_glue::WebThreadImpl* compositor_thread_impl =
        static_cast<webkit_glue::WebThreadImpl*>(compositor_thread);
    compositor_thread_message_loop_proxy_ =
        compositor_thread_impl->message_loop()->message_loop_proxy();
  } else {
    compositor_thread_message_loop_proxy_ = NULL;
  }
}

bool WebCompositorSupportImpl::isThreadingEnabled() {
  return compositor_thread_message_loop_proxy_;
}

void WebCompositorSupportImpl::shutdown() {
  DCHECK(initialized_);
  initialized_ = false;
  compositor_thread_message_loop_proxy_ = NULL;
}

WebKit::WebCompositorOutputSurface*
WebCompositorSupportImpl::createOutputSurfaceFor3D(
    WebKit::WebGraphicsContext3D* context) {
  scoped_ptr<WebKit::WebGraphicsContext3D> context3d = make_scoped_ptr(context);
  return new cc::OutputSurface(context3d.Pass());
}

WebKit::WebCompositorOutputSurface*
WebCompositorSupportImpl::createOutputSurfaceForSoftware() {
  scoped_ptr<cc::SoftwareOutputDevice> software_device =
      make_scoped_ptr(new cc::SoftwareOutputDevice);
  return new cc::OutputSurface(software_device.Pass());
}

WebLayer* WebCompositorSupportImpl::createLayer() {
  return new WebKit::WebLayerImpl();
}

WebContentLayer* WebCompositorSupportImpl::createContentLayer(
    WebContentLayerClient* client) {
  return new WebKit::WebContentLayerImpl(client);
}

WebExternalTextureLayer* WebCompositorSupportImpl::createExternalTextureLayer(
    WebExternalTextureLayerClient* client) {
  return new WebKit::WebExternalTextureLayerImpl(client);
}

WebKit::WebImageLayer* WebCompositorSupportImpl::createImageLayer() {
  return new WebKit::WebImageLayerImpl();
}

WebSolidColorLayer* WebCompositorSupportImpl::createSolidColorLayer() {
  return new WebKit::WebSolidColorLayerImpl();
}

WebVideoLayer* WebCompositorSupportImpl::createVideoLayer(
    WebKit::WebVideoFrameProvider* provider) {
  return new WebKit::WebVideoLayerImpl(provider);
}

WebScrollbarLayer* WebCompositorSupportImpl::createScrollbarLayer(
    WebScrollbar* scrollbar,
    WebScrollbarThemePainter painter,
    WebScrollbarThemeGeometry* geometry) {
  return new WebKit::WebScrollbarLayerImpl(scrollbar, painter, geometry);
}

WebAnimation* WebCompositorSupportImpl::createAnimation(
    const WebKit::WebAnimationCurve& curve,
    WebKit::WebAnimation::TargetProperty target,
    int animation_id) {
  return new WebKit::WebAnimationImpl(curve, target, animation_id);
}

WebFloatAnimationCurve* WebCompositorSupportImpl::createFloatAnimationCurve() {
  return new WebKit::WebFloatAnimationCurveImpl();
}

WebTransformAnimationCurve*
WebCompositorSupportImpl::createTransformAnimationCurve() {
  return new WebKit::WebTransformAnimationCurveImpl();
}

WebTransformOperations* WebCompositorSupportImpl::createTransformOperations() {
  return new WebTransformOperationsImpl();
}

}  // namespace webkit
