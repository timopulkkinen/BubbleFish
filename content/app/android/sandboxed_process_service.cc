// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/sandboxed_process_service.h"

#include <android/native_window_jni.h>
#include <cpu-features.h>

#include "base/android/jni_array.h"
#include "base/logging.h"
#include "base/posix/global_descriptors.h"
#include "content/common/android/scoped_java_surface.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/common/child_process.h"
#include "content/common/child_thread.h"
#include "content/common/gpu/gpu_surface_lookup.h"
#include "content/public/app/android_library_loader_hooks.h"
#include "content/public/common/content_descriptors.h"
#include "ipc/ipc_descriptors.h"
#include "jni/SandboxedProcessService_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::JavaIntArrayToIntVector;

namespace content {

namespace {

class SurfaceTexturePeerSandboxedImpl : public content::SurfaceTexturePeer,
                                        public content::GpuSurfaceLookup {
 public:
  // |service| is the instance of
  // org.chromium.content.app.SandboxedProcessService.
  explicit SurfaceTexturePeerSandboxedImpl(
      const base::android::ScopedJavaLocalRef<jobject>& service)
      : service_(service) {
    GpuSurfaceLookup::InitInstance(this);
  }

  virtual ~SurfaceTexturePeerSandboxedImpl() {
    GpuSurfaceLookup::InitInstance(NULL);
  }

  virtual void EstablishSurfaceTexturePeer(
      base::ProcessHandle pid,
      scoped_refptr<content::SurfaceTextureBridge> surface_texture_bridge,
      int primary_id,
      int secondary_id) {
    JNIEnv* env = base::android::AttachCurrentThread();
    content::Java_SandboxedProcessService_establishSurfaceTexturePeer(
        env, service_.obj(), pid,
        surface_texture_bridge->j_surface_texture().obj(), primary_id,
        secondary_id);
    CheckException(env);
  }

  virtual gfx::AcceleratedWidget AcquireNativeWidget(int surface_id) OVERRIDE {
    JNIEnv* env = base::android::AttachCurrentThread();
    ScopedJavaSurface surface(
        content::Java_SandboxedProcessService_getViewSurface(
        env, service_.obj(), surface_id));

    if (surface.j_surface().is_null())
      return NULL;

    ANativeWindow* native_window = ANativeWindow_fromSurface(
        env, surface.j_surface().obj());

    return native_window;
  }

 private:
  // The instance of org.chromium.content.app.SandboxedProcessService.
  base::android::ScopedJavaGlobalRef<jobject> service_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceTexturePeerSandboxedImpl);
};

// Chrome actually uses the renderer code path for all of its sandboxed
// processes such as renderers, plugins, etc.
void InternalInitSandboxedProcess(const std::vector<int>& file_ids,
                                  const std::vector<int>& file_fds,
                                  JNIEnv* env,
                                  jclass clazz,
                                  jobject context,
                                  jobject service_in,
                                  jint cpu_count,
                                  jlong cpu_features) {
  base::android::ScopedJavaLocalRef<jobject> service(env, service_in);

  // Set the CPU properties.
  android_setCpu(cpu_count, cpu_features);
  // Register the file descriptors.
  // This includes the IPC channel, the crash dump signals and resource related
  // files.
  DCHECK(file_fds.size() == file_ids.size());
  for (size_t i = 0; i < file_ids.size(); ++i)
    base::GlobalDescriptors::GetInstance()->Set(file_ids[i], file_fds[i]);

  content::SurfaceTexturePeer::InitInstance(
      new SurfaceTexturePeerSandboxedImpl(service));

}

void QuitSandboxMainThreadMessageLoop() {
  MessageLoop::current()->Quit();
}

}  // namespace <anonymous>

void InitSandboxedProcess(JNIEnv* env,
                          jclass clazz,
                          jobject context,
                          jobject service,
                          jintArray j_file_ids,
                          jintArray j_file_fds,
                          jint cpu_count,
                          jlong cpu_features) {
  std::vector<int> file_ids;
  std::vector<int> file_fds;
  JavaIntArrayToIntVector(env, j_file_ids, &file_ids);
  JavaIntArrayToIntVector(env, j_file_fds, &file_fds);

  InternalInitSandboxedProcess(
      file_ids, file_fds, env, clazz, context, service,
      cpu_count, cpu_features);
}

void ExitSandboxedProcess(JNIEnv* env, jclass clazz) {
  LOG(INFO) << "SandboxedProcessService: Exiting sandboxed process.";
  LibraryLoaderExitHook();
  _exit(0);
}

bool RegisterSandboxedProcessService(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ShutdownSandboxMainThread(JNIEnv* env, jobject obj) {
  ChildProcess* current_process = ChildProcess::current();
  if (!current_process)
    return;
  ChildThread* main_child_thread = current_process->main_thread();
  if (main_child_thread && main_child_thread->message_loop())
    main_child_thread->message_loop()->PostTask(FROM_HERE,
        base::Bind(&QuitSandboxMainThreadMessageLoop));
}

}  // namespace content
