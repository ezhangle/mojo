// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shell/android/android_handler.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/trace_event/trace_event.h"
#include "jni/AndroidHandler_jni.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "shell/android/run_android_application_function.h"
#include "shell/native_application_support.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetApplicationContext;

namespace shell {

namespace {

// This function loads the application library, sets the application context and
// thunks and calls into the application MojoMain. To ensure that the thunks are
// set correctly we keep it in the Mojo shell .so and pass the function pointer
// to the helper libbootstrap.so.
void RunAndroidApplication(JNIEnv* env,
                           jobject j_context,
                           uintptr_t tracing_id,
                           const base::FilePath& app_path,
                           jint j_handle) {
  mojo::InterfaceRequest<mojo::Application> application_request =
      mojo::MakeRequest<mojo::Application>(
          mojo::MakeScopedHandle(mojo::MessagePipeHandle(j_handle)));

  // Load the library, so that we can set the application context there if
  // needed.
  // TODO(vtl): We'd use a ScopedNativeLibrary, but it doesn't have .get()!
  base::NativeLibrary app_library =
      LoadNativeApplication(app_path, NativeApplicationCleanup::DELETE);
  if (!app_library)
    return;

  // Set the application context if needed. Most applications will need to
  // access the Android ApplicationContext in which they are run. If the
  // application library exports the InitApplicationContext function, we will
  // set it there.
  const char* init_application_context_name = "InitApplicationContext";
  typedef void (*InitApplicationContextFn)(
      const base::android::JavaRef<jobject>&);
  InitApplicationContextFn init_application_context =
      reinterpret_cast<InitApplicationContextFn>(
          base::GetFunctionPointerFromNativeLibrary(
              app_library, init_application_context_name));
  if (init_application_context) {
    base::android::ScopedJavaLocalRef<jobject> scoped_context(env, j_context);
    init_application_context(scoped_context);
  }

  // The application is ready to be run.
  TRACE_EVENT_ASYNC_END0("android_handler", "AndroidHandler::RunApplication",
                         tracing_id);

  // Run the application.
  RunNativeApplication(app_library, application_request.Pass());
  // TODO(vtl): See note about unloading and thread-local destructors above
  // declaration of |LoadNativeApplication()|.
  base::UnloadNativeLibrary(app_library);
}

}  // namespace

AndroidHandler::AndroidHandler() : content_handler_factory_(this) {
}

AndroidHandler::~AndroidHandler() {
}

void AndroidHandler::RunApplication(
    mojo::InterfaceRequest<mojo::Application> application_request,
    mojo::URLResponsePtr response) {
  JNIEnv* env = AttachCurrentThread();
  uintptr_t tracing_id = reinterpret_cast<uintptr_t>(this);
  TRACE_EVENT_ASYNC_BEGIN1("android_handler", "AndroidHandler::RunApplication",
                           tracing_id, "url", std::string(response->url));
  ScopedJavaLocalRef<jstring> j_archive_path =
      Java_AndroidHandler_getNewTempArchivePath(env, GetApplicationContext());
  base::FilePath archive_path(
      ConvertJavaStringToUTF8(env, j_archive_path.obj()));

  mojo::common::BlockingCopyToFile(response->body.Pass(), archive_path);
  RunAndroidApplicationFn run_android_application_fn = &RunAndroidApplication;
  Java_AndroidHandler_bootstrap(
      env, GetApplicationContext(), tracing_id, j_archive_path.obj(),
      application_request.PassMessagePipe().release().value(),
      reinterpret_cast<jlong>(run_android_application_fn));
}

void AndroidHandler::Initialize(mojo::ApplicationImpl* app) {
}

bool AndroidHandler::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService(&content_handler_factory_);
  connection->AddService(&intent_receiver_manager_factory_);
  return true;
}

bool RegisterAndroidHandlerJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace shell
