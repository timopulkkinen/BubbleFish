// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/browser/autofill_common_test.h"
#include "components/autofill/browser/autofill_metrics.h"
#include "components/autofill/common/form_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using content::BrowserThread;

class TestAutofillDialogView : public AutofillDialogView {
 public:
  TestAutofillDialogView() {}
  virtual ~TestAutofillDialogView() {}

  virtual void Show() OVERRIDE {}
  virtual void Hide() OVERRIDE {}
  virtual void UpdateNotificationArea() OVERRIDE {}
  virtual void UpdateAccountChooser() OVERRIDE {}
  virtual void UpdateButtonStrip() OVERRIDE {}
  virtual void UpdateSection(DialogSection section) OVERRIDE {}
  virtual void GetUserInput(DialogSection section, DetailOutputMap* output)
      OVERRIDE {}
  virtual string16 GetCvc() OVERRIDE { return string16(); }
  virtual bool UseBillingForShipping() OVERRIDE { return false; }
  virtual bool SaveDetailsInWallet() OVERRIDE { return false; }
  virtual bool SaveDetailsLocally() OVERRIDE { return false; }
  virtual const content::NavigationController* ShowSignIn() OVERRIDE {
    return NULL;
  }
  virtual void HideSignIn() OVERRIDE {}
  virtual void UpdateProgressBar(double value) OVERRIDE {}
  virtual void SubmitForTesting() OVERRIDE {}
  virtual void CancelForTesting() OVERRIDE {}

  MOCK_METHOD0(ModelChanged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogView);
};

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() {}
  virtual ~TestPersonalDataManager() {}

  void AddTestingProfile(AutofillProfile* profile) {
    profiles_.push_back(profile);
    FOR_EACH_OBSERVER(PersonalDataManagerObserver, observers_,
                      OnPersonalDataChanged());
  }

  virtual const std::vector<AutofillProfile*>& GetProfiles() OVERRIDE {
    return profiles_;
  }

 private:
  std::vector<AutofillProfile*> profiles_;
};

class TestAutofillDialogController : public AutofillDialogControllerImpl {
 public:
  TestAutofillDialogController(
      content::WebContents* contents,
      const FormData& form_structure,
      const GURL& source_url,
      const content::SSLStatus& ssl_status,
      const AutofillMetrics& metric_logger,
      const DialogType dialog_type,
      const base::Callback<void(const FormStructure*)>& callback)
      : AutofillDialogControllerImpl(contents,
                                     form_structure,
                                     source_url,
                                     ssl_status,
                                     metric_logger,
                                     dialog_type,
                                     callback) {}
  virtual ~TestAutofillDialogController() {}

  virtual AutofillDialogView* CreateView() OVERRIDE {
    return new TestAutofillDialogView();
  }

  TestAutofillDialogView* GetView() {
    return static_cast<TestAutofillDialogView*>(view());
  }

  TestPersonalDataManager* GetTestingManager() {
    return &test_manager_;
  }

 protected:
  virtual PersonalDataManager* GetManager() OVERRIDE {
    return &test_manager_;
  }

 private:
  TestPersonalDataManager test_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

class AutofillDialogControllerTest : public testing::Test {
 public:
  AutofillDialogControllerTest()
    : ui_thread_(BrowserThread::UI, &loop_),
      file_thread_(BrowserThread::FILE),
      file_blocking_thread_(BrowserThread::FILE_USER_BLOCKING),
      io_thread_(BrowserThread::IO) {
    file_thread_.Start();
    file_blocking_thread_.Start();
    io_thread_.StartIOThread();
  }

  virtual ~AutofillDialogControllerTest() {}

  // testing::Test implementation:
  virtual void SetUp() OVERRIDE {
    FormFieldData field;
    field.autocomplete_attribute = "cc-number";
    FormData form_data;
    form_data.fields.push_back(field);

    profile()->GetPrefs()->SetBoolean(
        prefs::kAutofillDialogPayWithoutWallet, true);
    profile()->CreateRequestContext();
    test_web_contents_.reset(
        content::WebContentsTester::CreateTestWebContents(profile(), NULL));

    base::Callback<void(const FormStructure*)> callback;
    controller_ = new TestAutofillDialogController(
        test_web_contents_.get(),
        form_data,
        GURL(),
        content::SSLStatus(),
        metric_logger_,
        DIALOG_TYPE_REQUEST_AUTOCOMPLETE,
        callback);
    controller_->Show();
  }

  virtual void TearDown() OVERRIDE {
    controller_->ViewClosed();
  }

 protected:
  TestAutofillDialogController* controller() { return controller_; }

  TestingProfile* profile() { return &profile_; }

 private:
  // A bunch of threads are necessary for classes like TestWebContents and
  // URLRequestContextGetter not to fall over.
  MessageLoopForUI loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread file_blocking_thread_;
  content::TestBrowserThread io_thread_;
  TestingProfile profile_;

  // The controller owns itself.
  TestAutofillDialogController* controller_;

  scoped_ptr<content::WebContents> test_web_contents_;

  // Must outlive the controller.
  AutofillMetrics metric_logger_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerTest);
};

}  // namespace

// This test makes sure nothing falls over when fields are being validity-
// checked.
TEST_F(AutofillDialogControllerTest, ValidityCheck) {
  const DialogSection sections[] = {
    SECTION_EMAIL,
    SECTION_CC,
    SECTION_BILLING,
    SECTION_CC_BILLING,
    SECTION_SHIPPING
  };

  for (size_t i = 0; i < arraysize(sections); ++i) {
    DialogSection section = sections[i];
    const DetailInputs& shipping_inputs =
        controller()->RequestedFieldsForSection(section);
    for (DetailInputs::const_iterator iter = shipping_inputs.begin();
         iter != shipping_inputs.end(); ++iter) {
      controller()->InputIsValid(iter->type, string16());
    }
  }
}

TEST_F(AutofillDialogControllerTest, AutofillProfiles) {
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  // Since the PersonalDataManager is empty, this should only have the
  // "add new" menu item.
  EXPECT_EQ(1, shipping_model->GetItemCount());

  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(2);

  // Empty profiles are ignored.
  AutofillProfile empty_profile(base::GenerateGUID());
  empty_profile.SetRawInfo(NAME_FULL, ASCIIToUTF16("John Doe"));
  controller()->GetTestingManager()->AddTestingProfile(&empty_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  EXPECT_EQ(1, shipping_model->GetItemCount());

  // A full profile should be picked up.
  AutofillProfile full_profile(autofill_test::GetFullProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  EXPECT_EQ(2, shipping_model->GetItemCount());
}

TEST_F(AutofillDialogControllerTest, AutofillProfileVariants) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);

  // Set up some variant data.
  AutofillProfile full_profile(autofill_test::GetFullProfile());
  std::vector<string16> names;
  names.push_back(ASCIIToUTF16("John Doe"));
  names.push_back(ASCIIToUTF16("Jane Doe"));
  full_profile.SetRawMultiInfo(EMAIL_ADDRESS, names);
  const string16 kEmail1 = ASCIIToUTF16("user@example.com");
  const string16 kEmail2 = ASCIIToUTF16("admin@example.com");
  std::vector<string16> emails;
  emails.push_back(kEmail1);
  emails.push_back(kEmail2);
  full_profile.SetRawMultiInfo(EMAIL_ADDRESS, emails);

  // Respect variants for the email address field only.
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  EXPECT_EQ(2, shipping_model->GetItemCount());
  ui::MenuModel* email_model =
      controller()->MenuModelForSection(SECTION_EMAIL);
  EXPECT_EQ(3, email_model->GetItemCount());

  email_model->ActivatedAt(0);
  EXPECT_EQ(kEmail1, controller()->SuggestionTextForSection(SECTION_EMAIL));
  email_model->ActivatedAt(1);
  EXPECT_EQ(kEmail2, controller()->SuggestionTextForSection(SECTION_EMAIL));

  controller()->EditClickedForSection(SECTION_EMAIL);
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_EMAIL);
  EXPECT_EQ(kEmail2, inputs[0].autofilled_value);
}

}  // namespace autofill
