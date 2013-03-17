// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_H_

#include <list>
#include <string>

class Status;
class WebView;

class Chrome {
 public:
  virtual ~Chrome() {}

  virtual std::string GetVersion() = 0;

  // Return a list of opened WebViews.
  virtual Status GetWebViews(std::list<WebView*>* web_views) = 0;

  // Returns whether a JavaScript dialog is open.
  virtual Status IsJavaScriptDialogOpen(bool* is_open) = 0;

  // Returns the message of the open JavaScript dialog.
  virtual Status GetJavaScriptDialogMessage(std::string* message) = 0;

  // Handles an open JavaScript dialog.
  virtual Status HandleJavaScriptDialog(bool accept,
                                        const std::string& prompt_text) = 0;

  // Get the operation system where Chrome is running.
  virtual std::string GetOperatingSystemName() = 0;

  // Quits Chrome.
  virtual Status Quit() = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_H_
