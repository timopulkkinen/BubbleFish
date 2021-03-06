// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SET_PASSPHRASE_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SET_PASSPHRASE_TEST_H_

#include "chrome/test/base/web_ui_browsertest.h"

class CommandLine;

class ManagedUserSetPassphraseTest : public WebUIBrowserTest {
 public:
  ManagedUserSetPassphraseTest();
  virtual ~ManagedUserSetPassphraseTest();

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedUserSetPassphraseTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SET_PASSPHRASE_TEST_H_
