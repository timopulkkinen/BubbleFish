// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resource_update.h"

#include "base/logging.h"

namespace cc {

ResourceUpdate ResourceUpdate::Create(PrioritizedResource* texture,
                                      const SkBitmap* bitmap,
                                      gfx::Rect content_rect,
                                      gfx::Rect source_rect,
                                      gfx::Vector2d dest_offset) {
    CHECK(content_rect.Contains(source_rect));
    ResourceUpdate update;
    update.texture = texture;
    update.bitmap = bitmap;
    update.content_rect = content_rect;
    update.source_rect = source_rect;
    update.dest_offset = dest_offset;
    return update;
}

ResourceUpdate ResourceUpdate::CreateFromPicture(PrioritizedResource* texture,
                                                 SkPicture* picture,
                                                 gfx::Rect content_rect,
                                                 gfx::Rect source_rect,
                                                 gfx::Vector2d dest_offset) {
    CHECK(content_rect.Contains(source_rect));
    ResourceUpdate update;
    update.texture = texture;
    update.picture = picture;
    update.content_rect = content_rect;
    update.source_rect = source_rect;
    update.dest_offset = dest_offset;
    return update;
}

ResourceUpdate::ResourceUpdate()
    : texture(NULL),
      bitmap(NULL),
      picture(NULL) {
}

ResourceUpdate::~ResourceUpdate() {
}

}  // namespace cc
