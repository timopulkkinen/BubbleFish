// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resource_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "cc/output_surface.h"
#include "cc/scoped_ptr_deque.h"
#include "cc/scoped_ptr_hash_map.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"

using namespace WebKit;

using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace cc {
namespace {

size_t textureSize(const gfx::Size& size, WGC3Denum format)
{
    unsigned int componentsPerPixel = 4;
    unsigned int bytesPerComponent = 1;
    return size.width() * size.height() * componentsPerPixel * bytesPerComponent;
}

struct Texture {
    Texture()
        : format(0)
        , filter(GL_NEAREST_MIPMAP_LINEAR)
    {
    }

    void reallocate(const gfx::Size& size, WGC3Denum format)
    {
        this->size = size;
        this->format = format;
        this->data.reset(new uint8_t[textureSize(size, format)]);
    }

    gfx::Size size;
    WGC3Denum format;
    WGC3Denum filter;
    scoped_array<uint8_t> data;
};

// Shared data between multiple ResourceProviderContext. This contains mailbox
// contents as well as information about sync points.
class ContextSharedData {
public:
    static scoped_ptr<ContextSharedData> Create() { return make_scoped_ptr(new ContextSharedData()); }

    unsigned insertSyncPoint() { return m_nextSyncPoint++; }

    void genMailbox(WGC3Dbyte* mailbox)
    {
        memset(mailbox, 0, sizeof(WGC3Dbyte[64]));
        memcpy(mailbox, &m_nextMailBox, sizeof(m_nextMailBox));
        ++m_nextMailBox;
    }

    void produceTexture(const WGC3Dbyte* mailboxName, unsigned syncPoint, scoped_ptr<Texture> texture)
    {
        unsigned mailbox = 0;
        memcpy(&mailbox, mailboxName, sizeof(mailbox));
        ASSERT_TRUE(mailbox && mailbox < m_nextMailBox);
        m_textures.set(mailbox, texture.Pass());
        ASSERT_LT(m_syncPointForMailbox[mailbox], syncPoint);
        m_syncPointForMailbox[mailbox] = syncPoint;
    }

    scoped_ptr<Texture> consumeTexture(const WGC3Dbyte* mailboxName, unsigned syncPoint)
    {
        unsigned mailbox = 0;
        memcpy(&mailbox, mailboxName, sizeof(mailbox));
        DCHECK(mailbox && mailbox < m_nextMailBox);

        // If the latest sync point the context has waited on is before the sync
        // point for when the mailbox was set, pretend we never saw that
        // produceTexture.
        if (m_syncPointForMailbox[mailbox] > syncPoint) {
            NOTREACHED();
            return scoped_ptr<Texture>();
        }
        return m_textures.take(mailbox);
    }

private:
    ContextSharedData()
        : m_nextSyncPoint(1)
        , m_nextMailBox(1)
    { }

    unsigned m_nextSyncPoint;
    unsigned m_nextMailBox;
    typedef ScopedPtrHashMap<unsigned, Texture> TextureMap;
    TextureMap m_textures;
    base::hash_map<unsigned, unsigned> m_syncPointForMailbox;
};

class ResourceProviderContext : public TestWebGraphicsContext3D {
public:
    static scoped_ptr<ResourceProviderContext> Create(ContextSharedData* sharedData) { return make_scoped_ptr(new ResourceProviderContext(Attributes(), sharedData)); }

    virtual unsigned insertSyncPoint()
    {
        unsigned syncPoint = m_sharedData->insertSyncPoint();
        // Commit the produceTextureCHROMIUM calls at this point, so that
        // they're associated with the sync point.
        for (PendingProduceTextureList::iterator it = m_pendingProduceTextures.begin(); it != m_pendingProduceTextures.end(); ++it)
            m_sharedData->produceTexture((*it)->mailbox, syncPoint, (*it)->texture.Pass());
        m_pendingProduceTextures.clear();
        return syncPoint;
    }

    virtual void waitSyncPoint(unsigned syncPoint)
    {
        m_lastWaitedSyncPoint = std::max(syncPoint, m_lastWaitedSyncPoint);
    }

    virtual void bindTexture(WGC3Denum target, WebGLId texture)
    {
      ASSERT_EQ(target, GL_TEXTURE_2D);
      ASSERT_TRUE(!texture || m_textures.find(texture) != m_textures.end());
      m_currentTexture = texture;
    }

    virtual WebGLId createTexture()
    {
        WebGLId id = TestWebGraphicsContext3D::createTexture();
        m_textures.add(id, make_scoped_ptr(new Texture));
        return id;
    }

    virtual void deleteTexture(WebGLId id)
    {
        TextureMap::iterator it = m_textures.find(id);
        ASSERT_FALSE(it == m_textures.end());
        m_textures.erase(it);
        if (m_currentTexture == id)
            m_currentTexture = 0;
    }

    virtual void texStorage2DEXT(WGC3Denum target, WGC3Dint levels, WGC3Duint internalformat,
                                 WGC3Dint width, WGC3Dint height)
    {
        ASSERT_TRUE(m_currentTexture);
        ASSERT_EQ(target, GL_TEXTURE_2D);
        ASSERT_EQ(levels, 1);
        WGC3Denum format = GL_RGBA;
        switch (internalformat) {
        case GL_RGBA8_OES:
            break;
        case GL_BGRA8_EXT:
            format = GL_BGRA_EXT;
            break;
        default:
            NOTREACHED();
        }
        allocateTexture(gfx::Size(width, height), format);
    }

    virtual void texImage2D(WGC3Denum target, WGC3Dint level, WGC3Denum internalformat, WGC3Dsizei width, WGC3Dsizei height, WGC3Dint border, WGC3Denum format, WGC3Denum type, const void* pixels)
    {
        ASSERT_TRUE(m_currentTexture);
        ASSERT_EQ(target, GL_TEXTURE_2D);
        ASSERT_FALSE(level);
        ASSERT_EQ(internalformat, format);
        ASSERT_FALSE(border);
        ASSERT_EQ(type, GL_UNSIGNED_BYTE);
        allocateTexture(gfx::Size(width, height), format);
        if (pixels)
            setPixels(0, 0, width, height, pixels);
    }

    virtual void texSubImage2D(WGC3Denum target, WGC3Dint level, WGC3Dint xoffset, WGC3Dint yoffset, WGC3Dsizei width, WGC3Dsizei height, WGC3Denum format, WGC3Denum type, const void* pixels)
    {
        ASSERT_TRUE(m_currentTexture);
        ASSERT_EQ(target, GL_TEXTURE_2D);
        ASSERT_FALSE(level);
        ASSERT_TRUE(m_textures.get(m_currentTexture));
        ASSERT_EQ(m_textures.get(m_currentTexture)->format, format);
        ASSERT_EQ(type, GL_UNSIGNED_BYTE);
        ASSERT_TRUE(pixels);
        setPixels(xoffset, yoffset, width, height, pixels);
    }

    virtual void texParameteri(WGC3Denum target, WGC3Denum param, WGC3Dint value)
    {
        ASSERT_TRUE(m_currentTexture);
        ASSERT_EQ(target, GL_TEXTURE_2D);
        Texture* texture = m_textures.get(m_currentTexture);
        ASSERT_TRUE(texture);
        if (param != GL_TEXTURE_MIN_FILTER)
            return;
        texture->filter = value;
    }

    virtual void genMailboxCHROMIUM(WGC3Dbyte* mailbox) { return m_sharedData->genMailbox(mailbox); }
    virtual void produceTextureCHROMIUM(WGC3Denum target, const WGC3Dbyte* mailbox)
    {
        ASSERT_TRUE(m_currentTexture);
        ASSERT_EQ(target, GL_TEXTURE_2D);

        // Delay moving the texture into the mailbox until the next
        // insertSyncPoint, so that it is not visible to other contexts that
        // haven't waited on that sync point.
        scoped_ptr<PendingProduceTexture> pending(new PendingProduceTexture);
        memcpy(pending->mailbox, mailbox, sizeof(pending->mailbox));
        pending->texture = m_textures.take(m_currentTexture);
        m_textures.set(m_currentTexture, scoped_ptr<Texture>());
        m_pendingProduceTextures.push_back(pending.Pass());
    }

    virtual void consumeTextureCHROMIUM(WGC3Denum target, const WGC3Dbyte* mailbox)
    {
        ASSERT_TRUE(m_currentTexture);
        ASSERT_EQ(target, GL_TEXTURE_2D);
        m_textures.set(m_currentTexture, m_sharedData->consumeTexture(mailbox, m_lastWaitedSyncPoint));
    }

    void getPixels(const gfx::Size& size, WGC3Denum format, uint8_t* pixels)
    {
        ASSERT_TRUE(m_currentTexture);
        Texture* texture = m_textures.get(m_currentTexture);
        ASSERT_TRUE(texture);
        ASSERT_EQ(texture->size, size);
        ASSERT_EQ(texture->format, format);
        memcpy(pixels, texture->data.get(), textureSize(size, format));
    }

    WGC3Denum getTextureFilter()
    {
        DCHECK(m_currentTexture);
        Texture* texture = m_textures.get(m_currentTexture);
        DCHECK(texture);
        return texture->filter;
    }

    int textureCount()
    {
        return m_textures.size();
    }

protected:
    ResourceProviderContext(const Attributes& attrs, ContextSharedData* sharedData)
        : TestWebGraphicsContext3D(attrs)
        , m_sharedData(sharedData)
        , m_currentTexture(0)
        , m_lastWaitedSyncPoint(0)
    { }

private:
    void allocateTexture(const gfx::Size& size, WGC3Denum format)
    {
        ASSERT_TRUE(m_currentTexture);
        Texture* texture = m_textures.get(m_currentTexture);
        ASSERT_TRUE(texture);
        texture->reallocate(size, format);
    }

    void setPixels(int xoffset, int yoffset, int width, int height, const void* pixels)
    {
        ASSERT_TRUE(m_currentTexture);
        Texture* texture = m_textures.get(m_currentTexture);
        ASSERT_TRUE(texture);
        ASSERT_TRUE(texture->data.get());
        ASSERT_TRUE(xoffset >= 0 && xoffset+width <= texture->size.width());
        ASSERT_TRUE(yoffset >= 0 && yoffset+height <= texture->size.height());
        ASSERT_TRUE(pixels);
        size_t inPitch = textureSize(gfx::Size(width, 1), texture->format);
        size_t outPitch = textureSize(gfx::Size(texture->size.width(), 1), texture->format);
        uint8_t* dest = texture->data.get() + yoffset * outPitch + textureSize(gfx::Size(xoffset, 1), texture->format);
        const uint8_t* src = static_cast<const uint8_t*>(pixels);
        for (int i = 0; i < height; ++i) {
            memcpy(dest, src, inPitch);
            dest += outPitch;
            src += inPitch;
        }
    }

    typedef ScopedPtrHashMap<WebGLId, Texture> TextureMap;
    struct PendingProduceTexture {
        WGC3Dbyte mailbox[64];
        scoped_ptr<Texture> texture;
    };
    typedef ScopedPtrDeque<PendingProduceTexture> PendingProduceTextureList;
    ContextSharedData* m_sharedData;
    WebGLId m_currentTexture;
    TextureMap m_textures;
    unsigned m_lastWaitedSyncPoint;
    PendingProduceTextureList m_pendingProduceTextures;
};

class ResourceProviderTest : public testing::TestWithParam<ResourceProvider::ResourceType> {
public:
    ResourceProviderTest()
        : m_sharedData(ContextSharedData::Create())
        , m_outputSurface(FakeOutputSurface::Create3d(ResourceProviderContext::Create(m_sharedData.get()).PassAs<WebKit::WebGraphicsContext3D>().PassAs<WebKit::WebGraphicsContext3D>()))
        , m_resourceProvider(ResourceProvider::Create(m_outputSurface.get()))
    {
        m_resourceProvider->set_default_resource_type(GetParam());
    }

    ResourceProviderContext* context() { return static_cast<ResourceProviderContext*>(m_outputSurface->context3d()); }

    void getResourcePixels(ResourceProvider::ResourceId id, const gfx::Size& size, WGC3Denum format, uint8_t* pixels)
    {
        if (GetParam() == ResourceProvider::GLTexture) {
            ResourceProvider::ScopedReadLockGL lockGL(m_resourceProvider.get(), id);
            ASSERT_NE(0U, lockGL.texture_id());
            context()->bindTexture(GL_TEXTURE_2D, lockGL.texture_id());
            context()->getPixels(size, format, pixels);
        } else if (GetParam() == ResourceProvider::Bitmap) {
            ResourceProvider::ScopedReadLockSoftware lockSoftware(m_resourceProvider.get(), id);
            memcpy(pixels, lockSoftware.sk_bitmap()->getPixels(), lockSoftware.sk_bitmap()->getSize());
        }
    }

    void setResourceFilter(ResourceProvider* resourceProvider, ResourceProvider::ResourceId id, WGC3Denum filter)
    {
        ResourceProvider::ScopedSamplerGL sampler(resourceProvider, id, GL_TEXTURE_2D, filter);
    }

    WGC3Denum getResourceFilter(ResourceProvider* resourceProvider, ResourceProvider::ResourceId id)
    {
        DCHECK_EQ(GetParam(), ResourceProvider::GLTexture);
        ResourceProvider::ScopedReadLockGL lockGL(resourceProvider, id);
        EXPECT_NE(0u, lockGL.texture_id());
        ResourceProviderContext* context = static_cast<ResourceProviderContext*>(resourceProvider->GraphicsContext3D());
        context->bindTexture(GL_TEXTURE_2D, lockGL.texture_id());
        return context->getTextureFilter();
    }

    void expectNumResources(int count)
    {
        EXPECT_EQ(count, static_cast<int>(m_resourceProvider->num_resources()));
        if (GetParam() == ResourceProvider::GLTexture)
            EXPECT_EQ(count, context()->textureCount());
    }

protected:
    scoped_ptr<ContextSharedData> m_sharedData;
    scoped_ptr<OutputSurface> m_outputSurface;
    scoped_ptr<ResourceProvider> m_resourceProvider;
};

TEST_P(ResourceProviderTest, Basic)
{
    gfx::Size size(1, 1);
    WGC3Denum format = GL_RGBA;
    size_t pixelSize = textureSize(size, format);
    ASSERT_EQ(4U, pixelSize);

    ResourceProvider::ResourceId id = m_resourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    expectNumResources(1);

    uint8_t data[4] = {1, 2, 3, 4};
    gfx::Rect rect(gfx::Point(), size);
    m_resourceProvider->SetPixels(id, data, rect, rect, gfx::Vector2d());

    uint8_t result[4] = {0};
    getResourcePixels(id, size, format, result);
    EXPECT_EQ(0, memcmp(data, result, pixelSize));

    m_resourceProvider->DeleteResource(id);
    expectNumResources(0);
}

TEST_P(ResourceProviderTest, Upload)
{
    gfx::Size size(2, 2);
    WGC3Denum format = GL_RGBA;
    size_t pixelSize = textureSize(size, format);
    ASSERT_EQ(16U, pixelSize);

    ResourceProvider::ResourceId id = m_resourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);

    uint8_t image[16] = {0};
    gfx::Rect imageRect(gfx::Point(), size);
    m_resourceProvider->SetPixels(id, image, imageRect, imageRect, gfx::Vector2d());

    for (uint8_t i = 0 ; i < pixelSize; ++i)
        image[i] = i;

    uint8_t result[16] = {0};
    {
        gfx::Rect sourceRect(0, 0, 1, 1);
        gfx::Vector2d destOffset(0, 0);
        m_resourceProvider->SetPixels(id, image, imageRect, sourceRect, destOffset);

        uint8_t expected[16] = {0, 1, 2, 3,   0, 0, 0, 0,
                                0, 0, 0, 0,   0, 0, 0, 0};
        getResourcePixels(id, size, format, result);
        EXPECT_EQ(0, memcmp(expected, result, pixelSize));
    }
    {
        gfx::Rect sourceRect(0, 0, 1, 1);
        gfx::Vector2d destOffset(1, 1);
        m_resourceProvider->SetPixels(id, image, imageRect, sourceRect, destOffset);

        uint8_t expected[16] = {0, 1, 2, 3,   0, 0, 0, 0,
                                0, 0, 0, 0,   0, 1, 2, 3};
        getResourcePixels(id, size, format, result);
        EXPECT_EQ(0, memcmp(expected, result, pixelSize));
    }
    {
        gfx::Rect sourceRect(1, 0, 1, 1);
        gfx::Vector2d destOffset(0, 1);
        m_resourceProvider->SetPixels(id, image, imageRect, sourceRect, destOffset);

        uint8_t expected[16] = {0, 1, 2, 3,   0, 0, 0, 0,
                                4, 5, 6, 7,   0, 1, 2, 3};
        getResourcePixels(id, size, format, result);
        EXPECT_EQ(0, memcmp(expected, result, pixelSize));
    }
    {
        gfx::Rect offsetImageRect(gfx::Point(100, 100), size);
        gfx::Rect sourceRect(100, 100, 1, 1);
        gfx::Vector2d destOffset(1, 0);
        m_resourceProvider->SetPixels(id, image, offsetImageRect, sourceRect, destOffset);

        uint8_t expected[16] = {0, 1, 2, 3,   0, 1, 2, 3,
                                4, 5, 6, 7,   0, 1, 2, 3};
        getResourcePixels(id, size, format, result);
        EXPECT_EQ(0, memcmp(expected, result, pixelSize));
    }


    m_resourceProvider->DeleteResource(id);
}

TEST_P(ResourceProviderTest, TransferResources)
{
    // Resource transfer is only supported with GL textures for now.
    if (GetParam() != ResourceProvider::GLTexture)
        return;

    scoped_ptr<OutputSurface> childOutputSurface(FakeOutputSurface::Create3d(ResourceProviderContext::Create(m_sharedData.get()).PassAs<WebKit::WebGraphicsContext3D>()));
    scoped_ptr<ResourceProvider> childResourceProvider(ResourceProvider::Create(childOutputSurface.get()));

    gfx::Size size(1, 1);
    WGC3Denum format = GL_RGBA;
    size_t pixelSize = textureSize(size, format);
    ASSERT_EQ(4U, pixelSize);

    ResourceProvider::ResourceId id1 = childResourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    uint8_t data1[4] = {1, 2, 3, 4};
    gfx::Rect rect(gfx::Point(), size);
    childResourceProvider->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

    ResourceProvider::ResourceId id2 = childResourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    uint8_t data2[4] = {5, 5, 5, 5};
    childResourceProvider->SetPixels(id2, data2, rect, rect, gfx::Vector2d());

    int childId = m_resourceProvider->CreateChild();

    {
        // Transfer some resources to the parent.
        ResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.push_back(id1);
        resourceIdsToTransfer.push_back(id2);
        TransferableResourceArray list;
        childResourceProvider->PrepareSendToParent(resourceIdsToTransfer, &list);
        ASSERT_EQ(2u, list.size());
        EXPECT_NE(0u, list[0].sync_point);
        EXPECT_NE(0u, list[1].sync_point);
        EXPECT_TRUE(childResourceProvider->InUseByConsumer(id1));
        EXPECT_TRUE(childResourceProvider->InUseByConsumer(id2));
        m_resourceProvider->ReceiveFromChild(childId, list);
    }

    EXPECT_EQ(2u, m_resourceProvider->num_resources());
    ResourceProvider::ResourceIdMap resourceMap = m_resourceProvider->GetChildToParentMap(childId);
    ResourceProvider::ResourceId mappedId1 = resourceMap[id1];
    ResourceProvider::ResourceId mappedId2 = resourceMap[id2];
    EXPECT_NE(0u, mappedId1);
    EXPECT_NE(0u, mappedId2);
    EXPECT_FALSE(m_resourceProvider->InUseByConsumer(id1));
    EXPECT_FALSE(m_resourceProvider->InUseByConsumer(id2));

    uint8_t result[4] = {0};
    getResourcePixels(mappedId1, size, format, result);
    EXPECT_EQ(0, memcmp(data1, result, pixelSize));

    getResourcePixels(mappedId2, size, format, result);
    EXPECT_EQ(0, memcmp(data2, result, pixelSize));

    {
        // Check that transfering again the same resource from the child to the
        // parent is a noop.
        ResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.push_back(id1);
        TransferableResourceArray list;
        childResourceProvider->PrepareSendToParent(resourceIdsToTransfer, &list);
        EXPECT_EQ(0u, list.size());
    }

    {
        // Transfer resources back from the parent to the child.
        ResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.push_back(mappedId1);
        resourceIdsToTransfer.push_back(mappedId2);
        TransferableResourceArray list;
        m_resourceProvider->PrepareSendToChild(childId, resourceIdsToTransfer, &list);
        ASSERT_EQ(2u, list.size());
        EXPECT_NE(0u, list[0].sync_point);
        EXPECT_NE(0u, list[1].sync_point);
        childResourceProvider->ReceiveFromParent(list);
    }
    EXPECT_FALSE(childResourceProvider->InUseByConsumer(id1));
    EXPECT_FALSE(childResourceProvider->InUseByConsumer(id2));

    ResourceProviderContext* childContext3D = static_cast<ResourceProviderContext*>(childOutputSurface->context3d());
    {
        ResourceProvider::ScopedReadLockGL lock(childResourceProvider.get(), id1);
        ASSERT_NE(0U, lock.texture_id());
        childContext3D->bindTexture(GL_TEXTURE_2D, lock.texture_id());
        childContext3D->getPixels(size, format, result);
        EXPECT_EQ(0, memcmp(data1, result, pixelSize));
    }
    {
        ResourceProvider::ScopedReadLockGL lock(childResourceProvider.get(), id2);
        ASSERT_NE(0U, lock.texture_id());
        childContext3D->bindTexture(GL_TEXTURE_2D, lock.texture_id());
        childContext3D->getPixels(size, format, result);
        EXPECT_EQ(0, memcmp(data2, result, pixelSize));
    }

    {
        // Transfer resources to the parent again.
        ResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.push_back(id1);
        resourceIdsToTransfer.push_back(id2);
        TransferableResourceArray list;
        childResourceProvider->PrepareSendToParent(resourceIdsToTransfer, &list);
        ASSERT_EQ(2u, list.size());
        EXPECT_NE(0u, list[0].sync_point);
        EXPECT_NE(0u, list[1].sync_point);
        EXPECT_TRUE(childResourceProvider->InUseByConsumer(id1));
        EXPECT_TRUE(childResourceProvider->InUseByConsumer(id2));
        m_resourceProvider->ReceiveFromChild(childId, list);
    }

    EXPECT_EQ(2u, m_resourceProvider->num_resources());
    m_resourceProvider->DestroyChild(childId);
    EXPECT_EQ(0u, m_resourceProvider->num_resources());
}

TEST_P(ResourceProviderTest, DeleteTransferredResources)
{
    // Resource transfer is only supported with GL textures for now.
    if (GetParam() != ResourceProvider::GLTexture)
        return;

    scoped_ptr<OutputSurface> childOutputSurface(FakeOutputSurface::Create3d(ResourceProviderContext::Create(m_sharedData.get()).PassAs<WebKit::WebGraphicsContext3D>()));
    scoped_ptr<ResourceProvider> childResourceProvider(ResourceProvider::Create(childOutputSurface.get()));

    gfx::Size size(1, 1);
    WGC3Denum format = GL_RGBA;
    size_t pixelSize = textureSize(size, format);
    ASSERT_EQ(4U, pixelSize);

    ResourceProvider::ResourceId id = childResourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    uint8_t data[4] = {1, 2, 3, 4};
    gfx::Rect rect(gfx::Point(), size);
    childResourceProvider->SetPixels(id, data, rect, rect, gfx::Vector2d());

    int childId = m_resourceProvider->CreateChild();

    {
        // Transfer some resource to the parent.
        ResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.push_back(id);
        TransferableResourceArray list;
        childResourceProvider->PrepareSendToParent(resourceIdsToTransfer, &list);
        ASSERT_EQ(1u, list.size());
        EXPECT_NE(0u, list[0].sync_point);
        EXPECT_TRUE(childResourceProvider->InUseByConsumer(id));
        m_resourceProvider->ReceiveFromChild(childId, list);
    }

    // Delete textures in the child, while they are transfered.
    childResourceProvider->DeleteResource(id);
    EXPECT_EQ(1u, childResourceProvider->num_resources());

    {
        // Transfer resources back from the parent to the child.
        ResourceProvider::ResourceIdMap resourceMap = m_resourceProvider->GetChildToParentMap(childId);
        ResourceProvider::ResourceId mappedId = resourceMap[id];
        EXPECT_NE(0u, mappedId);
        ResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.push_back(mappedId);
        TransferableResourceArray list;
        m_resourceProvider->PrepareSendToChild(childId, resourceIdsToTransfer, &list);
        ASSERT_EQ(1u, list.size());
        EXPECT_NE(0u, list[0].sync_point);
        childResourceProvider->ReceiveFromParent(list);
    }
    EXPECT_EQ(0u, childResourceProvider->num_resources());
}

TEST_P(ResourceProviderTest, TextureFilters)
{
    // Resource transfer is only supported with GL textures for now.
    if (GetParam() != ResourceProvider::GLTexture)
        return;

    scoped_ptr<OutputSurface> childOutputSurface(FakeOutputSurface::Create3d(ResourceProviderContext::Create(m_sharedData.get()).PassAs<WebKit::WebGraphicsContext3D>()));
    scoped_ptr<ResourceProvider> childResourceProvider(ResourceProvider::Create(childOutputSurface.get()));

    gfx::Size size(1, 1);
    WGC3Denum format = GL_RGBA;
    size_t pixelSize = textureSize(size, format);
    ASSERT_EQ(4U, pixelSize);

    ResourceProvider::ResourceId id = childResourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    uint8_t data[4] = {1, 2, 3, 4};
    gfx::Rect rect(gfx::Point(), size);
    childResourceProvider->SetPixels(id, data, rect, rect, gfx::Vector2d());
    EXPECT_EQ(GL_LINEAR, getResourceFilter(childResourceProvider.get(), id));
    setResourceFilter(childResourceProvider.get(), id, GL_NEAREST);
    EXPECT_EQ(GL_NEAREST, getResourceFilter(childResourceProvider.get(), id));

    int childId = m_resourceProvider->CreateChild();

    {
        // Transfer some resource to the parent.
        ResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.push_back(id);
        TransferableResourceArray list;
        childResourceProvider->PrepareSendToParent(resourceIdsToTransfer, &list);
        ASSERT_EQ(1u, list.size());
        EXPECT_EQ(GL_NEAREST, list[0].filter);
        m_resourceProvider->ReceiveFromChild(childId, list);
    }
    ResourceProvider::ResourceIdMap resourceMap = m_resourceProvider->GetChildToParentMap(childId);
    ResourceProvider::ResourceId mappedId = resourceMap[id];
    EXPECT_NE(0u, mappedId);
    EXPECT_EQ(GL_NEAREST, getResourceFilter(m_resourceProvider.get(), mappedId));
    setResourceFilter(m_resourceProvider.get(), mappedId, GL_LINEAR);
    EXPECT_EQ(GL_LINEAR, getResourceFilter(m_resourceProvider.get(), mappedId));
    {
        // Transfer resources back from the parent to the child.
        ResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.push_back(mappedId);
        TransferableResourceArray list;
        m_resourceProvider->PrepareSendToChild(childId, resourceIdsToTransfer, &list);
        ASSERT_EQ(1u, list.size());
        EXPECT_EQ(GL_LINEAR, list[0].filter);
        childResourceProvider->ReceiveFromParent(list);
    }
    EXPECT_EQ(GL_LINEAR, getResourceFilter(childResourceProvider.get(), id));
    setResourceFilter(childResourceProvider.get(), id, GL_NEAREST);
    EXPECT_EQ(GL_NEAREST, getResourceFilter(childResourceProvider.get(), id));
}

void ReleaseTextureMailbox(unsigned* releaseSyncPoint, unsigned syncPoint) {
    *releaseSyncPoint = syncPoint;
}

TEST_P(ResourceProviderTest, TransferMailboxResources)
{
    // Resource transfer is only supported with GL textures for now.
    if (GetParam() != ResourceProvider::GLTexture)
        return;
    unsigned texture = context()->createTexture();
    context()->bindTexture(GL_TEXTURE_2D, texture);
    uint8_t data[4] = {1, 2, 3, 4};
    context()->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data);
    gpu::Mailbox mailbox;
    context()->genMailboxCHROMIUM(mailbox.name);
    context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    unsigned syncPoint = context()->insertSyncPoint();

    // All the logic below assumes that the sync points are all positive.
    EXPECT_LT(0u, syncPoint);

    unsigned releaseSyncPoint = 0;
    TextureMailbox::ReleaseCallback callback = base::Bind(ReleaseTextureMailbox, &releaseSyncPoint);
    ResourceProvider::ResourceId resource = m_resourceProvider->CreateResourceFromTextureMailbox(TextureMailbox(mailbox, callback, syncPoint));
    EXPECT_EQ(1u, context()->textureCount());
    EXPECT_EQ(0u, releaseSyncPoint);

    {
        // Transfer the resource, expect the sync points to be consistent.
        ResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.push_back(resource);
        TransferableResourceArray list;
        m_resourceProvider->PrepareSendToParent(resourceIdsToTransfer, &list);
        ASSERT_EQ(1u, list.size());
        EXPECT_LE(syncPoint, list[0].sync_point);
        EXPECT_EQ(0u, memcmp(mailbox.name, list[0].mailbox.name, sizeof(mailbox.name)));
        EXPECT_EQ(0u, releaseSyncPoint);

        context()->waitSyncPoint(list[0].sync_point);
        unsigned otherTexture = context()->createTexture();
        context()->bindTexture(GL_TEXTURE_2D, otherTexture);
        context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
        uint8_t testData[4] = {0};
        context()->getPixels(gfx::Size(1, 1), GL_RGBA, testData);
        EXPECT_EQ(0u, memcmp(data, testData, sizeof(data)));
        context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
        context()->deleteTexture(otherTexture);
        list[0].sync_point = context()->insertSyncPoint();
        EXPECT_LT(0u, list[0].sync_point);

        // Receive the resource, then delete it, expect the sync points to be consistent.
        m_resourceProvider->ReceiveFromParent(list);
        EXPECT_EQ(1u, context()->textureCount());
        EXPECT_EQ(0u, releaseSyncPoint);

        m_resourceProvider->DeleteResource(resource);
        EXPECT_LE(list[0].sync_point, releaseSyncPoint);
    }


    // We're going to do the same thing as above, but testing the case where we
    // delete the resource before we receive it back.
    syncPoint = releaseSyncPoint;
    EXPECT_LT(0u, syncPoint);
    releaseSyncPoint = 0;
    resource = m_resourceProvider->CreateResourceFromTextureMailbox(TextureMailbox(mailbox, callback, syncPoint));
    EXPECT_EQ(1u, context()->textureCount());
    EXPECT_EQ(0u, releaseSyncPoint);

    {
        // Transfer the resource, expect the sync points to be consistent.
        ResourceProvider::ResourceIdArray resourceIdsToTransfer;
        resourceIdsToTransfer.push_back(resource);
        TransferableResourceArray list;
        m_resourceProvider->PrepareSendToParent(resourceIdsToTransfer, &list);
        ASSERT_EQ(1u, list.size());
        EXPECT_LE(syncPoint, list[0].sync_point);
        EXPECT_EQ(0u, memcmp(mailbox.name, list[0].mailbox.name, sizeof(mailbox.name)));
        EXPECT_EQ(0u, releaseSyncPoint);

        context()->waitSyncPoint(list[0].sync_point);
        unsigned otherTexture = context()->createTexture();
        context()->bindTexture(GL_TEXTURE_2D, otherTexture);
        context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
        uint8_t testData[4] = {0};
        context()->getPixels(gfx::Size(1, 1), GL_RGBA, testData);
        EXPECT_EQ(0u, memcmp(data, testData, sizeof(data)));
        context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
        context()->deleteTexture(otherTexture);
        list[0].sync_point = context()->insertSyncPoint();
        EXPECT_LT(0u, list[0].sync_point);

        // Delete the resource, which shouldn't do anything.
        m_resourceProvider->DeleteResource(resource);
        EXPECT_EQ(1u, context()->textureCount());
        EXPECT_EQ(0u, releaseSyncPoint);

        // Then receive the resource which should release the mailbox, expect the sync points to be consistent.
        m_resourceProvider->ReceiveFromParent(list);
        EXPECT_LE(list[0].sync_point, releaseSyncPoint);
    }

    context()->waitSyncPoint(releaseSyncPoint);
    context()->bindTexture(GL_TEXTURE_2D, texture);
    context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    context()->deleteTexture(texture);
}

class TextureStateTrackingContext : public TestWebGraphicsContext3D {
public:
    MOCK_METHOD2(bindTexture, void(WGC3Denum target, WebGLId texture));
    MOCK_METHOD3(texParameteri, void(WGC3Denum target, WGC3Denum pname, WGC3Dint param));

    // Force all textures to be "1" so we can test for them.
    virtual WebKit::WebGLId NextTextureId() { return 1; }
};

TEST_P(ResourceProviderTest, ScopedSampler)
{
    // Sampling is only supported for GL textures.
    if (GetParam() != ResourceProvider::GLTexture)
        return;

    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new TextureStateTrackingContext)));
    TextureStateTrackingContext* context = static_cast<TextureStateTrackingContext*>(outputSurface->context3d());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::Create(outputSurface.get()));

    gfx::Size size(1, 1);
    WGC3Denum format = GL_RGBA;
    int textureId = 1;

    // Check that the texture gets created with the right sampler settings.
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, textureId))
        .Times(2); // Once to create and once to allocate.
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_POOL_CHROMIUM, GL_TEXTURE_POOL_UNMANAGED_CHROMIUM));
    ResourceProvider::ResourceId id = resourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    resourceProvider->AllocateForTesting(id);

    // Creating a sampler with the default filter should not change any texture
    // parameters.
    {
        EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, textureId));
        ResourceProvider::ScopedSamplerGL sampler(resourceProvider.get(), id, GL_TEXTURE_2D, GL_LINEAR);
    }

    // Using a different filter should be reflected in the texture parameters.
    {
        EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, textureId));
        EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        ResourceProvider::ScopedSamplerGL sampler(resourceProvider.get(), id, GL_TEXTURE_2D, GL_NEAREST);
    }

    // Test resetting to the default filter.
    {
        EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, textureId));
        EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        ResourceProvider::ScopedSamplerGL sampler(resourceProvider.get(), id, GL_TEXTURE_2D, GL_LINEAR);
    }

    Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, ManagedResource)
{
    // Sampling is only supported for GL textures.
    if (GetParam() != ResourceProvider::GLTexture)
        return;

    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(new TextureStateTrackingContext)));
    TextureStateTrackingContext* context = static_cast<TextureStateTrackingContext*>(outputSurface->context3d());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::Create(outputSurface.get()));

    gfx::Size size(1, 1);
    WGC3Denum format = GL_RGBA;
    int textureId = 1;

    // Check that the texture gets created with the right sampler settings.
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, textureId));
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    EXPECT_CALL(*context, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_POOL_CHROMIUM, GL_TEXTURE_POOL_MANAGED_CHROMIUM));
    ResourceProvider::ResourceId id = resourceProvider->CreateManagedResource(size, format, ResourceProvider::TextureUsageAny);

    Mock::VerifyAndClearExpectations(context);
}

class AllocationTrackingContext3D : public TestWebGraphicsContext3D {
public:
    MOCK_METHOD0(createTexture, WebGLId());
    MOCK_METHOD1(deleteTexture, void(WebGLId texture_id));
    MOCK_METHOD2(bindTexture, void(WGC3Denum target, WebGLId texture));
    MOCK_METHOD9(texImage2D, void(WGC3Denum target, WGC3Dint level, WGC3Denum internalformat,
                                  WGC3Dsizei width, WGC3Dsizei height, WGC3Dint border, WGC3Denum format,
                                  WGC3Denum type, const void* pixels));
    MOCK_METHOD9(texSubImage2D, void(WGC3Denum target, WGC3Dint level, WGC3Dint xoffset, WGC3Dint yoffset,
                                     WGC3Dsizei width, WGC3Dsizei height, WGC3Denum format,
                                     WGC3Denum type, const void* pixels));
    MOCK_METHOD9(asyncTexImage2DCHROMIUM, void(WGC3Denum target, WGC3Dint level, WGC3Denum internalformat,
                                              WGC3Dsizei width, WGC3Dsizei height, WGC3Dint border, WGC3Denum format,
                                              WGC3Denum type, const void* pixels));
    MOCK_METHOD9(asyncTexSubImage2DCHROMIUM, void(WGC3Denum target, WGC3Dint level, WGC3Dint xoffset, WGC3Dint yoffset,
                                                  WGC3Dsizei width, WGC3Dsizei height, WGC3Denum format,
                                                  WGC3Denum type, const void* pixels));
    MOCK_METHOD1(waitAsyncTexImage2DCHROMIUM, void(WGC3Denum target));
};

TEST_P(ResourceProviderTest, TextureAllocation)
{
    // Only for GL textures.
    if (GetParam() != ResourceProvider::GLTexture)
        return;
    scoped_ptr<WebKit::WebGraphicsContext3D> mock_context(
        static_cast<WebKit::WebGraphicsContext3D*>(new NiceMock<AllocationTrackingContext3D>));
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(mock_context.Pass()));

    gfx::Size size(2, 2);
    gfx::Vector2d offset(0, 0);
    gfx::Rect rect(0, 0, 2, 2);
    WGC3Denum format = GL_RGBA;
    ResourceProvider::ResourceId id = 0;
    uint8_t pixels[16] = {0};
    int textureId = 123;

    AllocationTrackingContext3D* context = static_cast<AllocationTrackingContext3D*>(outputSurface->context3d());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::Create(outputSurface.get()));

    // Lazy allocation. Don't allocate when creating the resource.
    EXPECT_CALL(*context, createTexture()).WillOnce(Return(textureId));
    EXPECT_CALL(*context, deleteTexture(textureId)).Times(1);
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, textureId)).Times(1);
    EXPECT_CALL(*context, texImage2D(_,_,_,_,_,_,_,_,_)).Times(0);
    EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_,_,_,_,_,_,_,_,_)).Times(0);
    id = resourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    resourceProvider->DeleteResource(id);
    Mock::VerifyAndClearExpectations(context);

    // Do allocate when we set the pixels.
    EXPECT_CALL(*context, createTexture()).WillOnce(Return(textureId));
    EXPECT_CALL(*context, deleteTexture(textureId)).Times(1);
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, textureId)).Times(3);
    EXPECT_CALL(*context, texImage2D(_,_,_,2,2,_,_,_,_)).Times(1);
    EXPECT_CALL(*context, texSubImage2D(_,_,_,_,2,2,_,_,_)).Times(1);
    id = resourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    resourceProvider->SetPixels(id, pixels, rect, rect, offset);
    resourceProvider->DeleteResource(id);
    Mock::VerifyAndClearExpectations(context);

    // Same for setPixelsFromBuffer
    EXPECT_CALL(*context, createTexture()).WillOnce(Return(textureId));
    EXPECT_CALL(*context, deleteTexture(textureId)).Times(1);
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, textureId)).Times(3);
    EXPECT_CALL(*context, texImage2D(_,_,_,2,2,_,_,_,_)).Times(1);
    EXPECT_CALL(*context, texSubImage2D(_,_,_,_,2,2,_,_,_)).Times(1);
    id = resourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    resourceProvider->AcquirePixelBuffer(id);
    resourceProvider->SetPixelsFromBuffer(id);
    resourceProvider->ReleasePixelBuffer(id);
    resourceProvider->DeleteResource(id);
    Mock::VerifyAndClearExpectations(context);

    // Same for async version.
    EXPECT_CALL(*context, createTexture()).WillOnce(Return(textureId));
    EXPECT_CALL(*context, deleteTexture(textureId)).Times(1);
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, textureId)).Times(2);
    EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_,_,_,2,2,_,_,_,_)).Times(1);
    id = resourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    resourceProvider->AcquirePixelBuffer(id);
    resourceProvider->BeginSetPixels(id);
    resourceProvider->ReleasePixelBuffer(id);
    resourceProvider->DeleteResource(id);
    Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, ForcingAsyncUploadToComplete)
{
    // Only for GL textures.
    if (GetParam() != ResourceProvider::GLTexture)
        return;
    scoped_ptr<WebKit::WebGraphicsContext3D> mock_context(
        static_cast<WebKit::WebGraphicsContext3D*>(new NiceMock<AllocationTrackingContext3D>));
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(mock_context.Pass()));

    gfx::Size size(2, 2);
    WGC3Denum format = GL_RGBA;
    ResourceProvider::ResourceId id = 0;
    int textureId = 123;

    AllocationTrackingContext3D* context = static_cast<AllocationTrackingContext3D*>(outputSurface->context3d());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::Create(outputSurface.get()));

    EXPECT_CALL(*context, createTexture()).WillOnce(Return(textureId));
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, textureId)).Times(3);
    EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_,_,_,2,2,_,_,_,_)).Times(1);
    EXPECT_CALL(*context, waitAsyncTexImage2DCHROMIUM(GL_TEXTURE_2D)).Times(1);
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, 0)).Times(1);
    id = resourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    resourceProvider->AcquirePixelBuffer(id);
    resourceProvider->BeginSetPixels(id);
    resourceProvider->ForceSetPixelsToComplete(id);
    resourceProvider->ReleasePixelBuffer(id);
    Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, AbortForcedAsyncUpload)
{
    // Only for GL textures.
    if (GetParam() != ResourceProvider::GLTexture)
        return;
    scoped_ptr<WebKit::WebGraphicsContext3D> mock_context(
        static_cast<WebKit::WebGraphicsContext3D*>(new NiceMock<AllocationTrackingContext3D>));
    scoped_ptr<OutputSurface> outputSurface(FakeOutputSurface::Create3d(mock_context.Pass()));

    gfx::Size size(2, 2);
    WGC3Denum format = GL_RGBA;
    ResourceProvider::ResourceId id = 0;
    int textureId = 123;

    AllocationTrackingContext3D* context = static_cast<AllocationTrackingContext3D*>(outputSurface->context3d());
    scoped_ptr<ResourceProvider> resourceProvider(ResourceProvider::Create(outputSurface.get()));

    EXPECT_CALL(*context, createTexture()).WillRepeatedly(Return(textureId));
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, textureId)).Times(4);
    EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_,_,_,2,2,_,_,_,_)).Times(1);
    EXPECT_CALL(*context, waitAsyncTexImage2DCHROMIUM(GL_TEXTURE_2D)).Times(1);
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, 0)).Times(1);
    EXPECT_CALL(*context, deleteTexture(_)).Times(1);
    id = resourceProvider->CreateResource(size, format, ResourceProvider::TextureUsageAny);
    resourceProvider->AcquirePixelBuffer(id);
    resourceProvider->BeginSetPixels(id);
    resourceProvider->ForceSetPixelsToComplete(id);
    resourceProvider->AbortSetPixels(id);
    resourceProvider->ReleasePixelBuffer(id);
    Mock::VerifyAndClearExpectations(context);
}

INSTANTIATE_TEST_CASE_P(ResourceProviderTests,
                        ResourceProviderTest,
                        ::testing::Values(ResourceProvider::GLTexture,
                                          ResourceProvider::Bitmap));

}  // namespace
}  // namespace cc
