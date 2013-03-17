// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test.h"

#include "base/path_service.h"
#include "cc/compositor_frame_metadata.h"
#include "cc/gl_renderer.h"
#include "cc/output_surface.h"
#include "cc/resource_provider.h"
#include "cc/test/paths.h"
#include "cc/test/pixel_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "webkit/gpu/context_provider_in_process.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace cc {

class PixelTest::PixelTestRendererClient : public RendererClient {
 public:
  PixelTestRendererClient(gfx::Size device_viewport_size)
      : device_viewport_size_(device_viewport_size) {}

  // RendererClient implementation.
  virtual gfx::Size DeviceViewportSize() const OVERRIDE {
    return device_viewport_size_;
  }
  virtual const LayerTreeSettings& Settings() const OVERRIDE {
    return settings_;
  }
  virtual void DidLoseOutputSurface() OVERRIDE {}
  virtual void OnSwapBuffersComplete() OVERRIDE {}
  virtual void SetFullRootLayerDamage() OVERRIDE {}
  virtual void SetManagedMemoryPolicy(
      const ManagedMemoryPolicy& policy) OVERRIDE {}
  virtual void EnforceManagedMemoryPolicy(
      const ManagedMemoryPolicy& policy) OVERRIDE {}
  virtual bool HasImplThread() const OVERRIDE { return false; }
  virtual bool ShouldClearRootRenderPass() const OVERRIDE { return true; }
  virtual CompositorFrameMetadata MakeCompositorFrameMetadata() const
      OVERRIDE {
    return CompositorFrameMetadata();
  }

 private:
  gfx::Size device_viewport_size_;
  LayerTreeSettings settings_;
};

PixelTest::PixelTest() : device_viewport_size_(gfx::Size(200, 200)) {}

PixelTest::~PixelTest() {}

void PixelTest::SetUp() {
  CHECK(gfx::InitializeGLBindings(gfx::kGLImplementationOSMesaGL));

  scoped_ptr<webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl>
      context3d(
          new webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl);
  context3d->Initialize(WebKit::WebGraphicsContext3D::Attributes(), NULL);
  output_surface_.reset(new OutputSurface(
      context3d.PassAs<WebKit::WebGraphicsContext3D>()));
  resource_provider_ = ResourceProvider::Create(output_surface_.get());
  fake_client_ =
      make_scoped_ptr(new PixelTestRendererClient(device_viewport_size_));
  renderer_ = GLRenderer::Create(fake_client_.get(),
                                 output_surface_.get(),
                                 resource_provider_.get());

  scoped_refptr<webkit::gpu::ContextProviderInProcess> offscreen_contexts =
      webkit::gpu::ContextProviderInProcess::Create(
          webkit::gpu::ContextProviderInProcess::IN_PROCESS_COMMAND_BUFFER);
  ASSERT_TRUE(offscreen_contexts->BindToCurrentThread());
  resource_provider_->set_offscreen_context_provider(offscreen_contexts);
}

bool PixelTest::PixelsMatchReference(const base::FilePath& ref_file) {
  gfx::Rect device_viewport_rect(device_viewport_size_);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   device_viewport_rect.width(),
                   device_viewport_rect.height());
  bitmap.allocPixels();
  unsigned char* pixels = static_cast<unsigned char*>(bitmap.getPixels());
  renderer_->GetFramebufferPixels(pixels, device_viewport_rect);

  base::FilePath test_data_dir;
  if (!PathService::Get(cc::DIR_TEST_DATA, &test_data_dir))
    return false;

  // To rebaseline:
  //return WritePNGFile(bitmap, test_data_dir.Append(ref_file));

  return IsSameAsPNGFile(bitmap, test_data_dir.Append(ref_file));
}

}  // namespace cc
