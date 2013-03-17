// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/nine_patch_layer.h"

#include "cc/layer_tree_host.h"
#include "cc/occlusion_tracker.h"
#include "cc/overdraw_metrics.h"
#include "cc/prioritized_resource_manager.h"
#include "cc/resource_provider.h"
#include "cc/resource_update_queue.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test_common.h"
#include "cc/texture_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace cc {
namespace {

class MockLayerTreeHost : public LayerTreeHost {
public:
    MockLayerTreeHost()
        : LayerTreeHost(&m_fakeClient, LayerTreeSettings())
    {
        Initialize(scoped_ptr<Thread>(NULL));
    }

private:
    FakeLayerImplTreeHostClient m_fakeClient;
};


class NinePatchLayerTest : public testing::Test {
public:
    NinePatchLayerTest()
    {
    }

    Proxy* proxy() const { return layer_tree_host_->proxy(); }

protected:
    virtual void SetUp()
    {
        layer_tree_host_.reset(new MockLayerTreeHost);
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    }

    scoped_ptr<MockLayerTreeHost> layer_tree_host_;
};

TEST_F(NinePatchLayerTest, triggerFullUploadOnceWhenChangingBitmap)
{
    scoped_refptr<NinePatchLayer> testLayer = NinePatchLayer::Create();
    ASSERT_TRUE(testLayer);
    testLayer->SetIsDrawable(true);
    testLayer->SetBounds(gfx::Size(100, 100));

    layer_tree_host_->SetRootLayer(testLayer);
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    EXPECT_EQ(testLayer->layer_tree_host(), layer_tree_host_.get());

    layer_tree_host_->InitializeRendererIfNeeded();

    PriorityCalculator calculator;
    ResourceUpdateQueue queue;
    OcclusionTracker occlusionTracker(gfx::Rect(), false);

    // No bitmap set should not trigger any uploads.
    testLayer->SetTexturePriorities(calculator);
    testLayer->Update(&queue, &occlusionTracker, NULL);
    EXPECT_EQ(queue.fullUploadSize(), 0);
    EXPECT_EQ(queue.partialUploadSize(), 0);

    // Setting a bitmap set should trigger a single full upload.
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    bitmap.allocPixels();
    testLayer->SetBitmap(bitmap, gfx::Rect(5, 5, 1, 1));
    testLayer->SetTexturePriorities(calculator);
    testLayer->Update(&queue, &occlusionTracker, NULL);
    EXPECT_EQ(queue.fullUploadSize(), 1);
    EXPECT_EQ(queue.partialUploadSize(), 0);
    ResourceUpdate params = queue.takeFirstFullUpload();
    EXPECT_TRUE(params.texture != NULL);

    // Upload the texture.
    layer_tree_host_->contents_texture_manager()->setMaxMemoryLimitBytes(1024 * 1024);
    layer_tree_host_->contents_texture_manager()->prioritizeTextures();

    scoped_ptr<OutputSurface> outputSurface;
    scoped_ptr<ResourceProvider> resourceProvider;
    {
        DebugScopedSetImplThread implThread(proxy());
        DebugScopedSetMainThreadBlocked mainThreadBlocked(proxy());
        outputSurface = createFakeOutputSurface();
        resourceProvider = ResourceProvider::Create(outputSurface.get());
        params.texture->acquireBackingTexture(resourceProvider.get());
        ASSERT_TRUE(params.texture->haveBackingTexture());
    }

    // Nothing changed, so no repeated upload.
    testLayer->SetTexturePriorities(calculator);
    testLayer->Update(&queue, &occlusionTracker, NULL);
    EXPECT_EQ(queue.fullUploadSize(), 0);
    EXPECT_EQ(queue.partialUploadSize(), 0);

    {
        DebugScopedSetImplThread implThread(proxy());
        DebugScopedSetMainThreadBlocked mainThreadBlocked(proxy());
        layer_tree_host_->contents_texture_manager()->clearAllMemory(resourceProvider.get());
    }

    // Reupload after eviction
    testLayer->SetTexturePriorities(calculator);
    testLayer->Update(&queue, &occlusionTracker, NULL);
    EXPECT_EQ(queue.fullUploadSize(), 1);
    EXPECT_EQ(queue.partialUploadSize(), 0);

    // PrioritizedResourceManager clearing
    layer_tree_host_->contents_texture_manager()->unregisterTexture(params.texture);
    EXPECT_EQ(NULL, params.texture->resourceManager());
    testLayer->SetTexturePriorities(calculator);
    ResourceUpdateQueue queue2;
    testLayer->Update(&queue2, &occlusionTracker, NULL);
    EXPECT_EQ(queue2.fullUploadSize(), 1);
    EXPECT_EQ(queue2.partialUploadSize(), 0);
    params = queue2.takeFirstFullUpload();
    EXPECT_TRUE(params.texture != NULL);
    EXPECT_EQ(params.texture->resourceManager(), layer_tree_host_->contents_texture_manager());
}

}  // namespace
}  // namespace cc
