// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOCHECKOUT_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOCHECKOUT_BUBBLE_CONTROLLER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

class AutofillMetrics;

namespace autofill {

// The controller for the Autocheckout bubble UI. Implements a platform
// agnostic way to interact with the bubble, in addition to being a place
// for shared code such as display strings and tracking.
class AutocheckoutBubbleController {
 public:
  // |anchor_rect| is the anchor for the bubble UI. It is the bounds of an
  // input element in viewport space. |native_view| is the parent view of the
  // bubble. |callback| is invoked if the bubble is accepted. It brings up the
  // requestAutocomplete dialog to collect user input for Autocheckout.
  AutocheckoutBubbleController(const gfx::RectF& anchor_rect,
                               const gfx::NativeView& native_view,
                               const base::Callback<void(bool)>& callback);
  ~AutocheckoutBubbleController();

  static int AcceptTextID();
  static int CancelTextID();
  static int PromptTextID();

  void BubbleAccepted();
  void BubbleCanceled();
  void BubbleCreated() const;
  void BubbleDestroyed() const;

  const gfx::Rect& anchor_rect() const { return anchor_rect_; }

  const gfx::NativeView& native_view() {
    return native_view_;
  }

 protected:
  void set_metric_logger(AutofillMetrics* metric_logger);

  const AutofillMetrics* metric_logger() const {
    return metric_logger_.get();
  }

 private:
  // |anchor_rect_| is the anchor for the bubble UI. It is the bounds of an
  // input element in viewport space.
  gfx::Rect anchor_rect_;

  // |native_view| is the parent view of the bubble.
  gfx::NativeView native_view_;

  // |callback_| is invoked if the bubble is accepted.
  base::Callback<void(bool)> callback_;

  // For logging UMA metrics. Overridden by metrics tests.
  scoped_ptr<const AutofillMetrics> metric_logger_;

  // Whether or not the user interacted with the bubble.
  bool had_user_interaction_;

  DISALLOW_COPY_AND_ASSIGN(AutocheckoutBubbleController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOCHECKOUT_BUBBLE_CONTROLLER_H_

