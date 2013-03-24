// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer.h"
#include "ui/base/events/event_handler.h"
#include "ui/gfx/native_widget_types.h"

class BrowserView;

namespace views {
class MouseWatcher;
class View;
}

// Controller for an "immersive mode" similar to MacOS presentation mode where
// the top-of-window views are hidden until the mouse hits the top of the
// screen. The tab strip is optionally painted with miniature "tab indicator"
// rectangles.
class ImmersiveModeController : public ui::EventHandler {
 public:
  // Lock which keeps the top-of-window views revealed for the duration of its
  // lifetime. See GetRevealedLock() for more details.
  class RevealedLock {
   public:
    explicit RevealedLock(
        const base::WeakPtr<ImmersiveModeController>& controller);
    ~RevealedLock();

   private:
    base::WeakPtr<ImmersiveModeController> controller_;

    DISALLOW_COPY_AND_ASSIGN(RevealedLock);
  };

  ImmersiveModeController();
  virtual ~ImmersiveModeController();

  // Must initialize after browser view has a Widget and native window.
  void Init(BrowserView* browser_view);

  // Returns true if immersive mode should be used for fullscreen based on
  // command line flags.
  static bool UseImmersiveFullscreen();

  // Enables or disables immersive mode.
  void SetEnabled(bool enabled);
  bool enabled() const { return enabled_; }

  // See member comment below.
  bool hide_tab_indicators() const { return hide_tab_indicators_; }

  // True when the top views are hidden due to immersive mode.
  bool ShouldHideTopViews() const {
    return enabled_ && reveal_state_ == CLOSED;
  }

  // True when the top views are fully or partially visible.
  bool IsRevealed() const { return enabled_ && reveal_state_ != CLOSED; }

  // If the controller is temporarily revealing the top views ensures that
  // the reveal view's layer is on top and hence visible over web contents.
  void MaybeStackViewAtTop();

  // Shows the reveal view if immersive mode is enabled. Used when focus is
  // placed in the location bar, tools menu, etc.
  void MaybeStartReveal();

  // Immediately hides the reveal view, without animating.
  void CancelReveal();

  // Returns a lock which will keep the top-of-window views revealed for its
  // lifetime. Several locks can be obtained. When all of the locks are
  // destroyed, if immersive mode is enabled and there is nothing else keeping
  // the top-of-window views revealed, the top-of-window views will be closed.
  // This method always returns a valid lock regardless of whether immersive
  // mode is enabled. The lock's lifetime can span immersive mode being
  // enabled / disabled.
  // The caller takes ownership of the returned lock.
  RevealedLock* GetRevealedLock() WARN_UNUSED_RESULT;

  // Called when the reveal view's children lose focus, may end the reveal.
  void OnRevealViewLostFocus();

  // ui::EventHandler overrides:
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;

  // Testing interface.
  void SetHideTabIndicatorsForTest(bool hide);
  void StartRevealForTest();
  void OnRevealViewLostMouseForTest();

 private:
  enum Animate {
    ANIMATE_NO,
    ANIMATE_SLOW,
    ANIMATE_FAST,
  };
  enum RevealState {
    CLOSED,          // Top container only showing tabstrip, y = 0.
    SLIDING_OPEN,    // All views showing, y animating from -height to 0.
    REVEALED,        // All views showing, y = 0.
    SLIDING_CLOSED,  // All views showing, y animating from 0 to -height.
  };

  // Enables or disables observers for mouse move and window restore.
  void EnableWindowObservers(bool enable);

  // These methods are used to increment and decrement |revealed_lock_count_|.
  // If immersive mode is enabled, a transition from 1 to 0 in
  // |revealed_lock_count_| closes the top-of-window views and a transition
  // from 0 to 1 in |revealed_lock_count_| reveals the top-of-window views.
  void LockRevealedState();
  void UnlockRevealedState();

  // Returns true if a child of |browser_view_|->top_container() has focus.
  bool TopContainerChildHasFocus() const;

  // Temporarily reveals the top-of-window views while in immersive mode,
  // hiding them when the cursor exits the area of the top views. If |animate|
  // is not ANIMATE_NO, slides in the view, otherwise shows it immediately.
  void StartReveal(Animate animate);

  // Updates layout for |browser_view_| including window caption controls and
  // tab strip style |immersive_style|.
  void LayoutBrowserView(bool immersive_style);

  // Slides open the reveal view at the top of the screen.
  void AnimateSlideOpen();
  void OnSlideOpenAnimationCompleted();

  // Called when the mouse exits the reveal view area, may end the reveal.
  void OnRevealViewLostMouse();

  // Hides the top-of-window views if immersive mode is enabled and nothing is
  // keeping them revealed. Optionally animates.
  void MaybeEndReveal(Animate animate);

  // Hides the top-of-window views. Optionally animates.
  void EndReveal(Animate animate);

  // Slide out the reveal view.
  void AnimateSlideClosed(int duration_ms);
  void OnSlideClosedAnimationCompleted();

  // Browser view holding the views to be shown and hidden. Not owned.
  BrowserView* browser_view_;

  // True when in immersive mode.
  bool enabled_;

  // State machine for the revealed/closed animations.
  RevealState reveal_state_;

  int revealed_lock_count_;

  // True if the miniature "tab indicators" should be hidden in the main browser
  // view when immersive mode is enabled.
  bool hide_tab_indicators_;

  // Timer to track cursor being held at the top.
  base::OneShotTimer<ImmersiveModeController> top_timer_;

  // Mouse is hovering over the revealed view.
  bool reveal_hovered_;

  // Native window for the browser, needed to clean up observers.
  gfx::NativeWindow native_window_;

#if defined(USE_AURA)
  // Observer to disable immersive mode when window leaves the maximized state.
  class WindowObserver;
  scoped_ptr<WindowObserver> window_observer_;
#endif

  // Animation observers. They must be separate because animations can be
  // aborted and have their observers triggered at any time and the state
  // machine needs to know which animation completed.
  class AnimationObserver;
  scoped_ptr<AnimationObserver> slide_open_observer_;
  scoped_ptr<AnimationObserver> slide_closed_observer_;

  base::WeakPtrFactory<ImmersiveModeController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_
