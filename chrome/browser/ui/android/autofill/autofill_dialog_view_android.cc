// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_dialog_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "content/public/browser/web_contents.h"
#include "jni/AutofillDialogGlue_jni.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/android/window_android.h"

namespace autofill {

// AutofillDialogView ----------------------------------------------------------

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return new AutofillDialogViewAndroid(controller);
}

// AutofillDialogView ----------------------------------------------------------

AutofillDialogViewAndroid::AutofillDialogViewAndroid(
    AutofillDialogController* controller)
    : controller_(controller) {}

AutofillDialogViewAndroid::~AutofillDialogViewAndroid() {}

void AutofillDialogViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_AutofillDialogGlue_create(
      env,
      reinterpret_cast<jint>(this),
      WindowAndroidHelper::FromWebContents(controller_->web_contents())->
          GetWindowAndroid()->GetJavaObject().obj()));
}

void AutofillDialogViewAndroid::Hide() {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::UpdateNotificationArea() {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<DialogNotification> notifications =
      controller_->CurrentNotifications();
  const size_t count = notifications.size();
  ScopedJavaLocalRef<jobjectArray> notification_array =
      Java_AutofillDialogGlue_createAutofillDialogNotificationArray(
          env, count);
  for (size_t i = 0; i < count; ++i) {
    ScopedJavaLocalRef<jstring> text =
        base::android::ConvertUTF16ToJavaString(
            env, notifications[i].display_text());

    Java_AutofillDialogGlue_addToAutofillDialogNotificationArray(
        env,
        notification_array.obj(),
        i,
        static_cast<int>(notifications[i].GetBackgroundColor()),
        static_cast<int>(notifications[i].GetTextColor()),
        notifications[i].HasArrow(),
        notifications[i].HasCheckbox(),
        text.obj());
  }

  Java_AutofillDialogGlue_updateNotificationArea(env,
                                                 java_object_.obj(),
                                                 notification_array.obj());
}

void AutofillDialogViewAndroid::UpdateAccountChooser() {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::UpdateButtonStrip() {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::UpdateSection(DialogSection section) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const DetailInputs& updated_inputs =
      controller_->RequestedFieldsForSection(section);

  const size_t inputCount = updated_inputs.size();
  ScopedJavaLocalRef<jobjectArray> field_array =
      Java_AutofillDialogGlue_createAutofillDialogFieldArray(
          env, inputCount);

  for (size_t i = 0; i < inputCount; ++i) {
    const DetailInput& input = updated_inputs[i];

    ScopedJavaLocalRef<jstring> autofilled =
        base::android::ConvertUTF16ToJavaString(env, input.autofilled_value);

    string16 placeholder16;
    if (input.placeholder_text_rid > 0)
      placeholder16 = l10n_util::GetStringUTF16(input.placeholder_text_rid);
    ScopedJavaLocalRef<jstring> placeholder =
        base::android::ConvertUTF16ToJavaString(env, placeholder16);

    Java_AutofillDialogGlue_addToAutofillDialogFieldArray(
        env,
        field_array.obj(),
        i,
        reinterpret_cast<jint>(&input),
        input.type,
        placeholder.obj(),
        autofilled.obj());
  }

  ui::MenuModel* menuModel = controller_->MenuModelForSection(section);
  const int itemCount = menuModel->GetItemCount();
  ScopedJavaLocalRef<jobjectArray> menu_array =
      Java_AutofillDialogGlue_createAutofillDialogMenuItemArray(env,
                                                                itemCount);

  int checkedItem = -1;

  for (int i = 0; i < itemCount; ++i) {
    if (menuModel->IsItemCheckedAt(i))
      checkedItem = i;

    ScopedJavaLocalRef<jstring> line1 =
        base::android::ConvertUTF16ToJavaString(env, menuModel->GetLabelAt(i));
    ScopedJavaLocalRef<jstring> line2 =
        base::android::ConvertUTF16ToJavaString(env,
            menuModel->GetSublabelAt(i));

    ScopedJavaLocalRef<jobject> bitmap;
    gfx::Image icon;
    if (menuModel->GetIconAt(i, &icon)) {
      const SkBitmap& sk_icon = icon.AsBitmap();
      bitmap = gfx::ConvertToJavaBitmap(&sk_icon);
    }

    Java_AutofillDialogGlue_addToAutofillDialogMenuItemArray(
        env, menu_array.obj(), i, line1.obj(), line2.obj(), bitmap.obj());
  }

  Java_AutofillDialogGlue_updateSection(env,
                                        java_object_.obj(),
                                        section,
                                        controller_->SectionIsActive(section),
                                        field_array.obj(),
                                        menu_array.obj(),
                                        checkedItem);
}

void AutofillDialogViewAndroid::GetUserInput(DialogSection section,
                                             DetailOutputMap* output) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> fields =
      Java_AutofillDialogGlue_getSection(env, java_object_.obj(), section);
  DCHECK(fields.obj());

  const int arrayLength = env->GetArrayLength(fields.obj());

  for (int i = 0; i < arrayLength; ++i) {
    ScopedJavaLocalRef<jobject> field(
        env, env->GetObjectArrayElement(fields.obj(), i));
    DetailInput* input = reinterpret_cast<DetailInput*>(
        Java_AutofillDialogGlue_getFieldNativePointer(env, field.obj()));
    string16 value = base::android::ConvertJavaStringToUTF16(
        env, Java_AutofillDialogGlue_getFieldValue(env, field.obj()).obj());
    output->insert(std::make_pair(input, value));
  }
}

string16 AutofillDialogViewAndroid::GetCvc() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> cvc =
      Java_AutofillDialogGlue_getCvc(env, java_object_.obj());
  return base::android::ConvertJavaStringToUTF16(env, cvc.obj());
}

bool AutofillDialogViewAndroid::UseBillingForShipping() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AutofillDialogGlue_shouldUseBillingForShipping(
      env, java_object_.obj());
}

bool AutofillDialogViewAndroid::SaveDetailsInWallet() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AutofillDialogGlue_shouldSaveDetailsInWallet(env,
                                                           java_object_.obj());
}

bool AutofillDialogViewAndroid::SaveDetailsLocally() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AutofillDialogGlue_shouldSaveDetailsLocally(env,
                                                          java_object_.obj());
}

const content::NavigationController* AutofillDialogViewAndroid::ShowSignIn() {
  NOTIMPLEMENTED();
  return NULL;
}

void AutofillDialogViewAndroid::HideSignIn() {
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::UpdateProgressBar(double value) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillDialogGlue_updateProgressBar(env, java_object_.obj(), value);
}

void AutofillDialogViewAndroid::ModelChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillDialogGlue_modelChanged(env, java_object_.obj(), false);

  UpdateSection(SECTION_EMAIL);
  UpdateSection(SECTION_CC);
  UpdateSection(SECTION_BILLING);
  UpdateSection(SECTION_CC_BILLING);
  UpdateSection(SECTION_SHIPPING);
}

void AutofillDialogViewAndroid::SubmitForTesting() {
  controller_->OnSubmit();
}

void AutofillDialogViewAndroid::CancelForTesting() {
  controller_->OnCancel();
}

// Calls from Java to C++

void AutofillDialogViewAndroid::ItemSelected(JNIEnv* env, jobject obj,
                                             jint section, jint index) {
  ui::MenuModel* menuModel =
      controller_->MenuModelForSection(static_cast<DialogSection>(section));
  menuModel->ActivatedAt(index);
}

void AutofillDialogViewAndroid::AccountSelected(JNIEnv* env, jobject obj,
                                                jint index) {
  // TODO(aruslan): start using this call.
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::EditingStart(JNIEnv* env, jobject obj,
                                             jint section) {
  // TODO(aruslan): start using this call.
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::EditingComplete(JNIEnv* env, jobject obj,
                                           jint section) {
  // TODO(aruslan): start using this call.
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::EditingCancel(JNIEnv* env, jobject obj,
                                              jint section) {
  // TODO(aruslan): start using this call.
  NOTIMPLEMENTED();
}

void AutofillDialogViewAndroid::DialogSubmit(JNIEnv* env, jobject obj) {
  // TODO(aurimas): add validation step.
  controller_->OnSubmit();
}

void AutofillDialogViewAndroid::DialogCancel(JNIEnv* env, jobject obj) {
  controller_->OnCancel();
}

// static
bool AutofillDialogViewAndroid::RegisterAutofillDialogViewAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace autofill
