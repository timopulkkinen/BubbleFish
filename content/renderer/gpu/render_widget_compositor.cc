// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/render_widget_compositor.h"

#include <limits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "cc/layer.h"
#include "cc/layer_tree_debug_state.h"
#include "cc/layer_tree_host.h"
#include "cc/switches.h"
#include "cc/thread_impl.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/gpu/input_handler_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "ui/gl/gl_switches.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_to_ccinput_handler_adapter.h"

namespace cc {
class Layer;
}

using WebKit::WebFloatPoint;
using WebKit::WebSize;
using WebKit::WebRect;

namespace content {
namespace {

bool GetSwitchValueAsInt(
    const CommandLine& command_line,
    const std::string& switch_string,
    int min_value,
    int max_value,
    int* result) {
  std::string string_value = command_line.GetSwitchValueASCII(switch_string);
  int int_value;
  if (base::StringToInt(string_value, &int_value) &&
      int_value >= min_value && int_value <= max_value) {
    *result = int_value;
    return true;
  } else {
    LOG(WARNING) << "Failed to parse switch " << switch_string  << ": " <<
        string_value;
    return false;
  }
}

bool GetSwitchValueAsFloat(
    const CommandLine& command_line,
    const std::string& switch_string,
    float min_value,
    float max_value,
    float* result) {
  std::string string_value = command_line.GetSwitchValueASCII(switch_string);
  double double_value;
  if (base::StringToDouble(string_value, &double_value) &&
      double_value >= min_value && double_value <= max_value) {
    *result = static_cast<float>(double_value);
    return true;
  } else {
    LOG(WARNING) << "Failed to parse switch " << switch_string  << ": " <<
        string_value;
    return false;
  }
}


}  // namespace

// static
scoped_ptr<RenderWidgetCompositor> RenderWidgetCompositor::Create(
      RenderWidget* widget) {
  scoped_ptr<RenderWidgetCompositor> compositor(
      new RenderWidgetCompositor(widget));

  CommandLine* cmd = CommandLine::ForCurrentProcess();

  cc::LayerTreeSettings settings;
  settings.acceleratePainting =
      cmd->HasSwitch(switches::kEnableAcceleratedPainting);
  settings.renderVSyncEnabled = !cmd->HasSwitch(switches::kDisableGpuVsync);
  settings.perTilePaintingEnabled =
      cmd->HasSwitch(cc::switches::kEnablePerTilePainting);
  settings.acceleratedAnimationEnabled =
      !cmd->HasSwitch(cc::switches::kDisableThreadedAnimation);

  int default_tile_width = settings.defaultTileSize.width();
  if (cmd->HasSwitch(switches::kDefaultTileWidth)) {
    GetSwitchValueAsInt(*cmd, switches::kDefaultTileWidth, 1,
                        std::numeric_limits<int>::max(), &default_tile_width);
  }
  int default_tile_height = settings.defaultTileSize.height();
  if (cmd->HasSwitch(switches::kDefaultTileHeight)) {
    GetSwitchValueAsInt(*cmd, switches::kDefaultTileHeight, 1,
                        std::numeric_limits<int>::max(), &default_tile_height);
  }
  settings.defaultTileSize = gfx::Size(default_tile_width, default_tile_height);

  int max_untiled_layer_width = settings.maxUntiledLayerSize.width();
  if (cmd->HasSwitch(switches::kMaxUntiledLayerWidth)) {
    GetSwitchValueAsInt(*cmd, switches::kMaxUntiledLayerWidth, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_width);
  }
  int max_untiled_layer_height = settings.maxUntiledLayerSize.height();
  if (cmd->HasSwitch(switches::kMaxUntiledLayerHeight)) {
    GetSwitchValueAsInt(*cmd, switches::kMaxUntiledLayerHeight, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_height);
  }

  settings.maxUntiledLayerSize = gfx::Size(max_untiled_layer_width,
                                           max_untiled_layer_height);

  settings.rightAlignedSchedulingEnabled =
      cmd->HasSwitch(cc::switches::kEnableRightAlignedScheduling);
  settings.implSidePainting = cc::switches::IsImplSidePaintingEnabled();
  settings.useCheapnessEstimator =
      cmd->HasSwitch(cc::switches::kUseCheapnessEstimator);
  settings.useColorEstimator =
      cmd->HasSwitch(cc::switches::kUseColorEstimator);
  settings.predictionBenchmarking =
      cmd->HasSwitch(cc::switches::kEnablePredictionBenchmarking);

  settings.calculateTopControlsPosition =
      cmd->HasSwitch(cc::switches::kEnableTopControlsPositionCalculation);
  if (cmd->HasSwitch(cc::switches::kTopControlsHeight)) {
    std::string controls_height_str =
        cmd->GetSwitchValueASCII(cc::switches::kTopControlsHeight);
    double controls_height;
    if (base::StringToDouble(controls_height_str, &controls_height) &&
        controls_height > 0)
      settings.topControlsHeight = controls_height;
  }

  settings.compositorFrameMessage =
      cmd->HasSwitch(cc::switches::kEnableCompositorFrameMessage);

  if (settings.calculateTopControlsPosition &&
      (settings.topControlsHeight <= 0 || !settings.compositorFrameMessage)) {
    DCHECK(false) << "Top controls repositioning enabled without valid height "
                     "or compositorFrameMessage set.";
    settings.calculateTopControlsPosition = false;
  }

  if (cmd->HasSwitch(cc::switches::kTopControlsShowThreshold)) {
      std::string top_threshold_str =
          cmd->GetSwitchValueASCII(cc::switches::kTopControlsShowThreshold);
      double show_threshold;
      if (base::StringToDouble(top_threshold_str, &show_threshold) &&
          show_threshold >= 0.f && show_threshold <= 1.f)
        settings.topControlsShowThreshold = show_threshold;
  }

  if (cmd->HasSwitch(cc::switches::kTopControlsHideThreshold)) {
      std::string top_threshold_str =
          cmd->GetSwitchValueASCII(cc::switches::kTopControlsHideThreshold);
      double hide_threshold;
      if (base::StringToDouble(top_threshold_str, &hide_threshold) &&
          hide_threshold >= 0.f && hide_threshold <= 1.f)
        settings.topControlsHideThreshold = hide_threshold;
  }

  settings.partialSwapEnabled = widget->AllowPartialSwap() &&
      cmd->HasSwitch(cc::switches::kEnablePartialSwap);
  settings.backgroundColorInsteadOfCheckerboard =
      cmd->HasSwitch(cc::switches::kBackgroundColorInsteadOfCheckerboard);
  settings.showOverdrawInTracing =
      cmd->HasSwitch(cc::switches::kTraceOverdraw);

  settings.initialDebugState.showFPSCounter =
      cmd->HasSwitch(switches::kShowFPSCounter);
  settings.initialDebugState.showPaintRects =
      cmd->HasSwitch(switches::kShowPaintRects);
  settings.initialDebugState.showDebugBorders =
      cmd->HasSwitch(switches::kShowCompositedLayerBorders);
  settings.initialDebugState.showPlatformLayerTree =
      cmd->HasSwitch(switches::kShowCompositedLayerTree);

  settings.initialDebugState.showPropertyChangedRects =
      cmd->HasSwitch(cc::switches::kShowPropertyChangedRects);
  settings.initialDebugState.showSurfaceDamageRects =
      cmd->HasSwitch(cc::switches::kShowSurfaceDamageRects);
  settings.initialDebugState.showScreenSpaceRects =
      cmd->HasSwitch(cc::switches::kShowScreenSpaceRects);
  settings.initialDebugState.showReplicaScreenSpaceRects =
      cmd->HasSwitch(cc::switches::kShowReplicaScreenSpaceRects);
  settings.initialDebugState.showOccludingRects =
      cmd->HasSwitch(cc::switches::kShowOccludingRects);
  settings.initialDebugState.showNonOccludingRects =
      cmd->HasSwitch(cc::switches::kShowNonOccludingRects);
  settings.initialDebugState.setRecordRenderingStats(
      cmd->HasSwitch(switches::kEnableGpuBenchmarking));
  settings.initialDebugState.traceAllRenderedFrames =
      cmd->HasSwitch(cc::switches::kTraceAllRenderedFrames);

  if (cmd->HasSwitch(cc::switches::kSlowDownRasterScaleFactor)) {
    const int kMinSlowDownScaleFactor = 0;
    const int kMaxSlowDownScaleFactor = INT_MAX;
    GetSwitchValueAsInt(*cmd, cc::switches::kSlowDownRasterScaleFactor,
                        kMinSlowDownScaleFactor, kMaxSlowDownScaleFactor,
                        &settings.initialDebugState.slowDownRasterScaleFactor);
  }

  if (cmd->HasSwitch(cc::switches::kNumRasterThreads)) {
    const int kMinRasterThreads = 1;
    const int kMaxRasterThreads = 64;
    int num_raster_threads;
    if (GetSwitchValueAsInt(*cmd, cc::switches::kNumRasterThreads,
                            kMinRasterThreads, kMaxRasterThreads,
                            &num_raster_threads))
      settings.numRasterThreads = num_raster_threads;
  }

  if (cmd->HasSwitch(cc::switches::kLowResolutionContentsScaleFactor)) {
    const int kMinScaleFactor = settings.minimumContentsScale;
    const int kMaxScaleFactor = 1;
    GetSwitchValueAsFloat(*cmd,
                          cc::switches::kLowResolutionContentsScaleFactor,
                          kMinScaleFactor, kMaxScaleFactor,
                          &settings.lowResContentsScaleFactor);
  }

#if defined(OS_ANDROID)
  // TODO(danakj): Move these to the android code.
  settings.canUseLCDText = false;
  settings.maxPartialTextureUpdates = 0;
  settings.useLinearFadeScrollbarAnimator = true;
  settings.solidColorScrollbars = true;
  settings.solidColorScrollbarColor = SkColorSetARGB(128, 128, 128, 128);
  settings.solidColorScrollbarThicknessDIP = 3;
#endif

  if (!compositor->initialize(settings))
    return scoped_ptr<RenderWidgetCompositor>();

  return compositor.Pass();
}

RenderWidgetCompositor::RenderWidgetCompositor(RenderWidget* widget)
  : suppress_schedule_composite_(false),
    widget_(widget) {
}

RenderWidgetCompositor::~RenderWidgetCompositor() {}

void RenderWidgetCompositor::SetSuppressScheduleComposite(bool suppress) {
  if (suppress_schedule_composite_ == suppress)
    return;

  if (suppress)
    TRACE_EVENT_ASYNC_BEGIN0("gpu",
        "RenderWidgetCompositor::SetSuppressScheduleComposite", this);
  else
    TRACE_EVENT_ASYNC_END0("gpu",
        "RenderWidgetCompositor::SetSuppressScheduleComposite", this);
  suppress_schedule_composite_ = suppress;
}

void RenderWidgetCompositor::Animate(base::TimeTicks time) {
  layer_tree_host_->UpdateAnimations(time);
}

void RenderWidgetCompositor::Composite(base::TimeTicks frame_begin_time) {
  layer_tree_host_->Composite(frame_begin_time);
}

void RenderWidgetCompositor::GetRenderingStats(cc::RenderingStats* stats) {
  layer_tree_host_->CollectRenderingStats(stats);
}

skia::RefPtr<SkPicture> RenderWidgetCompositor::CapturePicture() {
  return layer_tree_host_->CapturePicture();
}

void RenderWidgetCompositor::EnableHidingTopControls(bool enable) {
  layer_tree_host_->EnableHidingTopControls(enable);
}

bool RenderWidgetCompositor::initialize(cc::LayerTreeSettings settings) {
  scoped_ptr<cc::Thread> impl_thread;
  scoped_refptr<base::MessageLoopProxy> compositor_message_loop_proxy =
      RenderThreadImpl::current()->compositor_message_loop_proxy();
  threaded_ = !!compositor_message_loop_proxy;
  if (threaded_) {
    impl_thread = cc::ThreadImpl::createForDifferentThread(
        compositor_message_loop_proxy);
  }
  layer_tree_host_ = cc::LayerTreeHost::Create(this,
                                               settings,
                                               impl_thread.Pass());
  return layer_tree_host_;
}

void RenderWidgetCompositor::setSurfaceReady() {
  layer_tree_host_->SetSurfaceReady();
}

void RenderWidgetCompositor::setRootLayer(const WebKit::WebLayer& layer) {
  layer_tree_host_->SetRootLayer(
      static_cast<const WebKit::WebLayerImpl*>(&layer)->layer());
}

void RenderWidgetCompositor::clearRootLayer() {
  layer_tree_host_->SetRootLayer(scoped_refptr<cc::Layer>());
}

void RenderWidgetCompositor::setViewportSize(
    const WebSize& layout_viewport_size,
    const WebSize& device_viewport_size) {
  layer_tree_host_->SetViewportSize(layout_viewport_size, device_viewport_size);
}

WebSize RenderWidgetCompositor::layoutViewportSize() const {
  return layer_tree_host_->layout_viewport_size();
}

WebSize RenderWidgetCompositor::deviceViewportSize() const {
  return layer_tree_host_->device_viewport_size();
}

WebFloatPoint RenderWidgetCompositor::adjustEventPointForPinchZoom(
    const WebFloatPoint& point) const {
  return point;
}

void RenderWidgetCompositor::setDeviceScaleFactor(float device_scale) {
  layer_tree_host_->SetDeviceScaleFactor(device_scale);
}

float RenderWidgetCompositor::deviceScaleFactor() const {
  return layer_tree_host_->device_scale_factor();
}

void RenderWidgetCompositor::setBackgroundColor(WebKit::WebColor color) {
  layer_tree_host_->set_background_color(color);
}

void RenderWidgetCompositor::setHasTransparentBackground(bool transparent) {
  layer_tree_host_->set_has_transparent_background(transparent);
}

void RenderWidgetCompositor::setVisible(bool visible) {
  layer_tree_host_->SetVisible(visible);
}

void RenderWidgetCompositor::setPageScaleFactorAndLimits(
    float page_scale_factor, float minimum, float maximum) {
  layer_tree_host_->SetPageScaleFactorAndLimits(
      page_scale_factor, minimum, maximum);
}

void RenderWidgetCompositor::startPageScaleAnimation(
    const WebKit::WebPoint& destination,
    bool use_anchor,
    float new_page_scale,
    double duration_sec) {
  base::TimeDelta duration = base::TimeDelta::FromMicroseconds(
      duration_sec * base::Time::kMicrosecondsPerSecond);
  layer_tree_host_->StartPageScaleAnimation(
      gfx::Vector2d(destination.x, destination.y),
      use_anchor,
      new_page_scale,
      duration);
}

void RenderWidgetCompositor::setNeedsAnimate() {
  layer_tree_host_->SetNeedsAnimate();
}

void RenderWidgetCompositor::setNeedsRedraw() {
  if (threaded_)
    layer_tree_host_->SetNeedsAnimate();
  else
    widget_->scheduleAnimation();
}

bool RenderWidgetCompositor::commitRequested() const {
  return layer_tree_host_->CommitRequested();
}

void RenderWidgetCompositor::didStopFlinging() {
  layer_tree_host_->DidStopFlinging();
}

void RenderWidgetCompositor::registerForAnimations(WebKit::WebLayer* layer) {
  cc::Layer* cc_layer = static_cast<WebKit::WebLayerImpl*>(layer)->layer();
  cc_layer->layer_animation_controller()->SetAnimationRegistrar(
      layer_tree_host_->animation_registrar());
}

bool RenderWidgetCompositor::compositeAndReadback(
    void *pixels, const WebRect& rect_in_device_viewport) {
  return layer_tree_host_->CompositeAndReadback(pixels,
                                                rect_in_device_viewport);
}

void RenderWidgetCompositor::finishAllRendering() {
  layer_tree_host_->FinishAllRendering();
}

void RenderWidgetCompositor::setDeferCommits(bool defer_commits) {
  layer_tree_host_->SetDeferCommits(defer_commits);
}

void RenderWidgetCompositor::setShowFPSCounter(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.showFPSCounter = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::setShowPaintRects(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.showPaintRects = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::setShowDebugBorders(bool show) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.showDebugBorders = show;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::setContinuousPaintingEnabled(bool enabled) {
  cc::LayerTreeDebugState debug_state = layer_tree_host_->debug_state();
  debug_state.continuousPainting = enabled;
  layer_tree_host_->SetDebugState(debug_state);
}

void RenderWidgetCompositor::willBeginFrame() {
  widget_->InstrumentWillBeginFrame();
  widget_->willBeginCompositorFrame();
}

void RenderWidgetCompositor::didBeginFrame() {
  widget_->InstrumentDidBeginFrame();
}

void RenderWidgetCompositor::animate(double monotonic_frame_begin_time) {
  widget_->webwidget()->animate(monotonic_frame_begin_time);
}

void RenderWidgetCompositor::layout() {
  widget_->webwidget()->layout();
}

void RenderWidgetCompositor::applyScrollAndScale(gfx::Vector2d scroll_delta,
                                                 float page_scale) {
  widget_->webwidget()->applyScrollAndScale(scroll_delta, page_scale);
}

scoped_ptr<cc::OutputSurface> RenderWidgetCompositor::createOutputSurface() {
  return widget_->CreateOutputSurface();
}

void RenderWidgetCompositor::didRecreateOutputSurface(bool success) {
  if (!success)
    widget_->webwidget()->didExitCompositingMode();
}

scoped_ptr<cc::InputHandler> RenderWidgetCompositor::createInputHandler() {
  scoped_ptr<cc::InputHandler> ret;
  scoped_ptr<WebKit::WebInputHandler> web_handler(
      widget_->webwidget()->createInputHandler());
  if (web_handler)
     ret = WebKit::WebToCCInputHandlerAdapter::create(web_handler.Pass());
  return ret.Pass();
}

void RenderWidgetCompositor::willCommit() {
  widget_->InstrumentWillComposite();
}

void RenderWidgetCompositor::didCommit() {
  widget_->DidCommitCompositorFrame();
  widget_->didBecomeReadyForAdditionalInput();
}

void RenderWidgetCompositor::didCommitAndDrawFrame() {
  widget_->didCommitAndDrawCompositorFrame();
}

void RenderWidgetCompositor::didCompleteSwapBuffers() {
  widget_->didCompleteSwapBuffers();
}

void RenderWidgetCompositor::scheduleComposite() {
  if (!suppress_schedule_composite_)
    widget_->scheduleComposite();
}

scoped_refptr<cc::ContextProvider>
RenderWidgetCompositor::OffscreenContextProviderForMainThread() {
  return RenderThreadImpl::current()->OffscreenContextProviderForMainThread();
}

scoped_refptr<cc::ContextProvider>
RenderWidgetCompositor::OffscreenContextProviderForCompositorThread() {
  return RenderThreadImpl::current()->
      OffscreenContextProviderForCompositorThread();
}

}  // namespace content
