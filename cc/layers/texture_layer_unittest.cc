// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include <string>

#include "base/callback.h"
#include "cc/base/thread.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace cc {
namespace {

class MockLayerTreeHost : public LayerTreeHost {
 public:
  MockLayerTreeHost(LayerTreeHostClient* client)
      : LayerTreeHost(client, LayerTreeSettings()) {
    Initialize(scoped_ptr<Thread>(NULL));
  }

  MOCK_METHOD0(AcquireLayerTextures, void());
  MOCK_METHOD0(SetNeedsCommit, void());
};

class TextureLayerTest : public testing::Test {
 public:
  TextureLayerTest()
      : fake_client_(
          FakeLayerTreeHostClient(FakeLayerTreeHostClient::DIRECT_3D)),
        host_impl_(&proxy_) {}

 protected:
  virtual void SetUp() {
    layer_tree_host_.reset(new MockLayerTreeHost(&fake_client_));
  }

  virtual void TearDown() {
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());
    EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());

    layer_tree_host_->SetRootLayer(NULL);
    layer_tree_host_.reset();
  }

  scoped_ptr<MockLayerTreeHost> layer_tree_host_;
  FakeImplProxy proxy_;
  FakeLayerTreeHostClient fake_client_;
  FakeLayerTreeHostImpl host_impl_;
};

TEST_F(TextureLayerTest, SyncImplWhenChangingTextureId) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::Create(NULL);
  ASSERT_TRUE(test_layer);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(1);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AtLeast(1));
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(2);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AtLeast(1));
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(0);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

TEST_F(TextureLayerTest, SyncImplWhenDrawing) {
  gfx::RectF dirty_rect(0.f, 0.f, 1.f, 1.f);

  scoped_refptr<TextureLayer> test_layer = TextureLayer::Create(NULL);
  ASSERT_TRUE(test_layer);
  scoped_ptr<TextureLayerImpl> impl_layer;
  impl_layer = TextureLayerImpl::Create(host_impl_.active_tree(), 1, false);
  ASSERT_TRUE(impl_layer);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  test_layer->SetTextureId(1);
  test_layer->SetIsDrawable(true);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  EXPECT_EQ(test_layer->layer_tree_host(), layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(1);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  test_layer->WillModifyTexture();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  test_layer->SetNeedsDisplayRect(dirty_rect);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(1);
  test_layer->PushPropertiesTo(impl_layer.get());  // fake commit
  test_layer->SetIsDrawable(false);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Verify that non-drawable layers don't signal the compositor,
  // except for the first draw after last commit, which must acquire
  // the texture.
  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(1);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  test_layer->WillModifyTexture();
  test_layer->SetNeedsDisplayRect(dirty_rect);
  test_layer->PushPropertiesTo(impl_layer.get());  // fake commit
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  // Second draw with layer in non-drawable state: no texture
  // acquisition.
  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(0);
  test_layer->WillModifyTexture();
  test_layer->SetNeedsDisplayRect(dirty_rect);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

TEST_F(TextureLayerTest, SyncImplWhenRemovingFromTree) {
  scoped_refptr<Layer> root_layer = Layer::Create();
  ASSERT_TRUE(root_layer);
  scoped_refptr<Layer> child_layer = Layer::Create();
  ASSERT_TRUE(child_layer);
  root_layer->AddChild(child_layer);
  scoped_refptr<TextureLayer> test_layer = TextureLayer::Create(NULL);
  ASSERT_TRUE(test_layer);
  test_layer->SetTextureId(0);
  child_layer->AddChild(test_layer);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AnyNumber());
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(root_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->RemoveFromParent();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  child_layer->AddChild(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureId(1);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(AtLeast(1));
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->RemoveFromParent();
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
}

class MockMailboxCallback {
 public:
  MOCK_METHOD2(Release, void(const std::string& mailbox, unsigned sync_point));
};

struct CommonMailboxObjects {
  CommonMailboxObjects()
      : mailbox_name1_(64, '1'),
        mailbox_name2_(64, '2'),
        sync_point1_(1),
        sync_point2_(2) {
    release_mailbox1_ = base::Bind(&MockMailboxCallback::Release,
                                   base::Unretained(&mock_callback_),
                                   mailbox_name1_);
    release_mailbox2_ = base::Bind(&MockMailboxCallback::Release,
                                   base::Unretained(&mock_callback_),
                                   mailbox_name2_);
    gpu::Mailbox m1;
    m1.SetName(reinterpret_cast<const int8*>(mailbox_name1_.data()));
    mailbox1_ = TextureMailbox(m1, release_mailbox1_, sync_point1_);
    gpu::Mailbox m2;
    m2.SetName(reinterpret_cast<const int8*>(mailbox_name2_.data()));
    mailbox2_ = TextureMailbox(m2, release_mailbox2_, sync_point2_);
  }

  std::string mailbox_name1_;
  std::string mailbox_name2_;
  MockMailboxCallback mock_callback_;
  TextureMailbox::ReleaseCallback release_mailbox1_;
  TextureMailbox::ReleaseCallback release_mailbox2_;
  TextureMailbox mailbox1_;
  TextureMailbox mailbox2_;
  unsigned sync_point1_;
  unsigned sync_point2_;
};

class TextureLayerWithMailboxTest : public TextureLayerTest {
 protected:
  virtual void TearDown() {
    Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
    EXPECT_CALL(test_data_.mock_callback_,
                Release(test_data_.mailbox_name1_,
                        test_data_.sync_point1_)).Times(1);
    TextureLayerTest::TearDown();
  }

  CommonMailboxObjects test_data_;
};

TEST_F(TextureLayerWithMailboxTest, ReplaceMailboxOnMainThreadBeforeCommit) {
  scoped_refptr<TextureLayer> test_layer = TextureLayer::CreateForMailbox();
  ASSERT_TRUE(test_layer);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AnyNumber());
  layer_tree_host_->SetRootLayer(test_layer);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureMailbox(test_data_.mailbox1_);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, test_data_.sync_point1_))
      .Times(1);
  test_layer->SetTextureMailbox(test_data_.mailbox2_);
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(*layer_tree_host_, AcquireLayerTextures()).Times(0);
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name2_, test_data_.sync_point2_))
      .Times(1);
  test_layer->SetTextureMailbox(TextureMailbox());
  Mock::VerifyAndClearExpectations(layer_tree_host_.get());
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test destructor.
  EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times(AtLeast(1));
  test_layer->SetTextureMailbox(test_data_.mailbox1_);
}

class TextureLayerImplWithMailboxThreadedCallback : public LayerTreeTest {
 public:
  TextureLayerImplWithMailboxThreadedCallback()
      : callback_count_(0),
        commit_count_(0) {}

  // Make sure callback is received on main and doesn't block the impl thread.
  void ReleaseCallback(unsigned sync_point) {
    EXPECT_EQ(true, proxy()->IsMainThread());
    ++callback_count_;
  }

  void SetMailbox(char mailbox_char) {
    TextureMailbox mailbox(
        std::string(64, mailbox_char),
        base::Bind(
            &TextureLayerImplWithMailboxThreadedCallback::ReleaseCallback,
            base::Unretained(this)));
    layer_->SetTextureMailbox(mailbox);
  }

  virtual void BeginTest() OVERRIDE {
    gfx::Size bounds(100, 100);
    root_ = Layer::Create();
    root_->SetAnchorPoint(gfx::PointF());
    root_->SetBounds(bounds);

    layer_ = TextureLayer::CreateForMailbox();
    layer_->SetIsDrawable(true);
    layer_->SetAnchorPoint(gfx::PointF());
    layer_->SetBounds(bounds);

    root_->AddChild(layer_);
    layer_tree_host()->SetRootLayer(root_);
    layer_tree_host()->SetViewportSize(bounds, bounds);
    SetMailbox('1');
    EXPECT_EQ(0, callback_count_);

    // Case #1: change mailbox before the commit. The old mailbox should be
    // released immediately.
    SetMailbox('2');
    EXPECT_EQ(1, callback_count_);
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    ++commit_count_;
    switch (commit_count_) {
      case 1:
        // Case #2: change mailbox after the commit (and draw), where the
        // layer draws. The old mailbox should be released during the next
        // commit.
        SetMailbox('3');
        EXPECT_EQ(1, callback_count_);
        break;
      case 2:
        // Old mailbox was released, task was posted, but won't execute
        // until this didCommit returns.
        // TODO(piman): fix this.
        EXPECT_EQ(1, callback_count_);
        layer_tree_host()->SetNeedsCommit();
        break;
      case 3:
        EXPECT_EQ(2, callback_count_);
        // Case #3: change mailbox when the layer doesn't draw. The old
        // mailbox should be released during the next commit.
        layer_->SetBounds(gfx::Size());
        SetMailbox('4');
        break;
      case 4:
        // Old mailbox was released, task was posted, but won't execute
        // until this didCommit returns.
        // TODO(piman): fix this.
        EXPECT_EQ(2, callback_count_);
        layer_tree_host()->SetNeedsCommit();
        break;
      case 5:
        EXPECT_EQ(3, callback_count_);
        // Case #4: release mailbox that was committed but never drawn. The
        // old mailbox should be released during the next commit.
        layer_->SetTextureMailbox(TextureMailbox());
        break;
      case 6:
        // Old mailbox was released, task was posted, but won't execute
        // until this didCommit returns.
        // TODO(piman): fix this.
        EXPECT_EQ(3, callback_count_);
        layer_tree_host()->SetNeedsCommit();
        break;
      case 7:
        EXPECT_EQ(4, callback_count_);
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

 private:
  int callback_count_;
  int commit_count_;
  scoped_refptr<Layer> root_;
  scoped_refptr<TextureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerImplWithMailboxThreadedCallback);

class TextureLayerImplWithMailboxTest : public TextureLayerTest {
 protected:
  TextureLayerImplWithMailboxTest()
      : fake_client_(
          FakeLayerTreeHostClient(FakeLayerTreeHostClient::DIRECT_3D)) {}

  virtual void SetUp() {
    TextureLayerTest::SetUp();
    layer_tree_host_.reset(new MockLayerTreeHost(&fake_client_));
    EXPECT_TRUE(host_impl_.InitializeRenderer(CreateFakeOutputSurface()));
  }

  CommonMailboxObjects test_data_;
  FakeLayerTreeHostClient fake_client_;
};

TEST_F(TextureLayerImplWithMailboxTest, TestImplLayerCallbacks) {
  host_impl_.CreatePendingTree();
  scoped_ptr<TextureLayerImpl> pending_layer;
  pending_layer = TextureLayerImpl::Create(host_impl_.pending_tree(), 1, true);
  ASSERT_TRUE(pending_layer);

  scoped_ptr<LayerImpl> activeLayer(
      pending_layer->CreateLayerImpl(host_impl_.active_tree()));
  ASSERT_TRUE(activeLayer);

  pending_layer->SetTextureMailbox(test_data_.mailbox1_);

  // Test multiple commits without an activation.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, test_data_.sync_point1_))
      .Times(1);
  pending_layer->SetTextureMailbox(test_data_.mailbox2_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test callback after activation.
  pending_layer->PushPropertiesTo(activeLayer.get());
  activeLayer->DidBecomeActive();

  EXPECT_CALL(test_data_.mock_callback_, Release(_, _)).Times(0);
  pending_layer->SetTextureMailbox(test_data_.mailbox1_);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  EXPECT_CALL(test_data_.mock_callback_, Release(test_data_.mailbox_name2_, _))
      .Times(1);
  pending_layer->PushPropertiesTo(activeLayer.get());
  activeLayer->DidBecomeActive();
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test resetting the mailbox.
  EXPECT_CALL(test_data_.mock_callback_, Release(test_data_.mailbox_name1_, _))
      .Times(1);
  pending_layer->SetTextureMailbox(TextureMailbox());
  pending_layer->PushPropertiesTo(activeLayer.get());
  activeLayer->DidBecomeActive();
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);

  // Test destructor.
  EXPECT_CALL(test_data_.mock_callback_,
              Release(test_data_.mailbox_name1_, test_data_.sync_point1_))
      .Times(1);
  pending_layer->SetTextureMailbox(test_data_.mailbox1_);
}

TEST_F(TextureLayerImplWithMailboxTest,
       TestDestructorCallbackOnCreatedResource) {
  scoped_ptr<TextureLayerImpl> impl_layer;
  impl_layer = TextureLayerImpl::Create(host_impl_.active_tree(), 1, true);
  ASSERT_TRUE(impl_layer);

  EXPECT_CALL(test_data_.mock_callback_, Release(test_data_.mailbox_name1_, _))
      .Times(1);
  impl_layer->SetTextureMailbox(test_data_.mailbox1_);
  impl_layer->WillDraw(host_impl_.active_tree()->resource_provider());
  impl_layer->DidDraw(host_impl_.active_tree()->resource_provider());
  impl_layer->SetTextureMailbox(TextureMailbox());
}

TEST_F(TextureLayerImplWithMailboxTest, TestCallbackOnInUseResource) {
  ResourceProvider* provider = host_impl_.active_tree()->resource_provider();
  ResourceProvider::ResourceId id =
      provider->CreateResourceFromTextureMailbox(test_data_.mailbox1_);
  provider->AllocateForTesting(id);

  // Transfer some resources to the parent.
  ResourceProvider::ResourceIdArray resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(id);
  TransferableResourceArray list;
  provider->PrepareSendToParent(resource_ids_to_transfer, &list);
  EXPECT_TRUE(provider->InUseByConsumer(id));
  EXPECT_CALL(test_data_.mock_callback_, Release(_, _)).Times(0);
  provider->DeleteResource(id);
  Mock::VerifyAndClearExpectations(&test_data_.mock_callback_);
  EXPECT_CALL(test_data_.mock_callback_, Release(test_data_.mailbox_name1_, _))
      .Times(1);
  provider->ReceiveFromParent(list);
}

}  // namespace
}  // namespace cc
