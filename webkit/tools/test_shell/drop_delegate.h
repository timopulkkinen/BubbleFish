// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that implements BaseDropTarget for the test shell webview delegate.

#ifndef WEBKIT_TOOLS_TEST_SHELL_DROP_DELEGATE_H_
#define WEBKIT_TOOLS_TEST_SHELL_DROP_DELEGATE_H_

#include "ui/base/dragdrop/drop_target_win.h"

namespace WebKit {
class WebView;
}

class TestDropDelegate : public ui::DropTargetWin {
 public:
  TestDropDelegate(HWND source_hwnd, WebKit::WebView* webview);

 protected:
  // BaseDropTarget methods
  virtual DWORD OnDragEnter(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect);
  virtual DWORD OnDragOver(IDataObject* data_object,
                           DWORD key_state,
                           POINT cursor_position,
                           DWORD effect);
  virtual void OnDragLeave(IDataObject* data_object);
  virtual DWORD OnDrop(IDataObject* data_object,
                       DWORD key_state,
                       POINT cursor_position,
                       DWORD effect);

 private:
  WebKit::WebView* webview_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_DROP_DELEGATE_H_
