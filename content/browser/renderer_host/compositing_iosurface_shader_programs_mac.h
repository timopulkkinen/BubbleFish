// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_SHADER_PROGRAMS_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_SHADER_PROGRAMS_MAC_H_

#include <OpenGL/gl.h>

#include "base/basictypes.h"

namespace content {

// Provides caching of the compile-and-link step for shader programs at runtime
// since, once compiled and linked, the programs can be shared.  Callers invoke
// one of the UseXXX() methods within a GL context to glUseProgram() the program
// and have its uniform variables bound with the given parameters.
class CompositingIOSurfaceShaderPrograms {
 public:
  CompositingIOSurfaceShaderPrograms();
  ~CompositingIOSurfaceShaderPrograms();

  // Reset the cache, deleting any references to currently-cached shader
  // programs.  This must be called within an active OpenGL context just before
  // destruction.
  void Reset();

  // Begin using the "blit" program, which is set up to sample the texture at
  // GL_TEXTURE_0 + |texture_unit_offset|.  Returns false on error.
  bool UseBlitProgram(int texture_unit_offset);

  // Begin using the program that just draws solid white very efficiently.
  // Returns false on error.
  bool UseSolidWhiteProgram();

  // Begin using one of the two RGB-to-YV12 color conversion programs, as
  // specified by |pass_number| 1 or 2.  The programs will sample the texture at
  // GL_TEXTURE0 + |texture_unit_offset|, and account for scaling in the X
  // direction by |texel_scale_x|.  Returns false on error.
  bool UseRGBToYV12Program(
      int pass_number, int texture_unit_offset, float texel_scale_x);

 private:
  enum { kNumShaderPrograms = 4 };

  // Helper methods to cache uniform variable locations.
  GLuint GetShaderProgram(int which);
  void BindUniformTextureVariable(int which, int texture_unit_offset);
  void BindUniformTexelScaleXVariable(int which, float texel_scale_x);

  // Cached values for previously-compiled/linked shader programs, and the
  // locations of their uniform variables.
  GLuint shader_programs_[kNumShaderPrograms];
  GLint texture_var_locations_[kNumShaderPrograms];
  GLint texel_scale_x_var_locations_[kNumShaderPrograms];

  DISALLOW_COPY_AND_ASSIGN(CompositingIOSurfaceShaderPrograms);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_SHADER_PROGRAMS_MAC_H_
