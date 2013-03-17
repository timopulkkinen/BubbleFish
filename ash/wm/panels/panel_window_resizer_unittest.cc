// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/panels/panel_window_resizer.h"

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_launcher_delegate.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

class PanelWindowResizerTest : public test::AshTestBase {
 public:
  PanelWindowResizerTest() {}
  virtual ~PanelWindowResizerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    test::ShellTestApi test_api(Shell::GetInstance());
    model_ = test_api.launcher_model();

    panel_layout_manager_ = static_cast<internal::PanelLayoutManager*>(
        GetPanelContainer()->layout_manager());
    launcher_bounds_ = panel_layout_manager_->launcher()->shelf_widget()->
        GetWindowBoundsInScreen();
  }

  virtual void TearDown() OVERRIDE {
    AshTestBase::TearDown();
  }

 protected:
  gfx::Point CalculateDragPoint(const PanelWindowResizer& resizer,
                                int delta_x,
                                int delta_y) const {
    gfx::Point location = resizer.GetInitialLocationInParentForTest();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  aura::Window* CreatePanelWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        NULL,
        aura::client::WINDOW_TYPE_PANEL,
        0,
        bounds);
    test::TestLauncherDelegate* launcher_delegate =
        test::TestLauncherDelegate::instance();
    launcher_delegate->AddLauncherItem(window);
    PanelLayoutManager* manager =
        static_cast<PanelLayoutManager*>(GetPanelContainer()->layout_manager());
    manager->Relayout();
    return window;
  }

  aura::Window* GetPanelContainer() {
    return Shell::GetContainer(
        Shell::GetPrimaryRootWindow(),
        internal::kShellWindowId_PanelContainer);
  }

  static PanelWindowResizer* CreatePanelWindowResizer(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) {
    return static_cast<PanelWindowResizer*>(CreateWindowResizer(
        window, point_in_parent, window_component).release());
  }

  void DragStart(aura::Window* window) {
    resizer_.reset(CreatePanelWindowResizer(window, gfx::Point(), HTCAPTION));
    ASSERT_TRUE(resizer_.get());
  }

  void DragMove(int dx, int dy) {
    resizer_->Drag(CalculateDragPoint(*resizer_, dx, dy), 0);
  }

  void DragEnd() {
    resizer_->CompleteDrag(0);
    resizer_.reset();
  }

  // Test dragging the panel slightly, then detaching, and then reattaching
  // dragging out by the vector (dx, dy).
  void DetachReattachTest(int dx, int dy) {
    scoped_ptr<aura::Window> window(
        CreatePanelWindow(gfx::Rect(0, 0, 201, 201)));
    EXPECT_TRUE(window->GetProperty(kPanelAttachedKey));
    DragStart(window.get());
    gfx::Rect initial_bounds = window->bounds();

    // Drag the panel slightly. The window should still be snapped to the
    // launcher.
    DragMove(dx * 5, dy * 5);
    EXPECT_EQ(initial_bounds.x(), window->bounds().x());
    EXPECT_EQ(initial_bounds.y(), window->bounds().y());

    // Drag further out and the window should now move to the cursor.
    DragMove(dx * 100, dy * 100);
    EXPECT_EQ(initial_bounds.x() + dx * 100, window->bounds().x());
    EXPECT_EQ(initial_bounds.y() + dy * 100, window->bounds().y());

    // The panel should be detached when the drag completes.
    DragEnd();
    EXPECT_FALSE(window->GetProperty(kPanelAttachedKey));

    DragStart(window.get());
    // Drag the panel down.
    DragMove(dx * -95, dy * -95);
    // Release the mouse and the panel should be reattached.
    DragEnd();

    // The panel should be reattached and have snapped to the launcher.
    EXPECT_TRUE(window->GetProperty(kPanelAttachedKey));
    EXPECT_EQ(initial_bounds.x(), window->bounds().x());
    EXPECT_EQ(initial_bounds.y(), window->bounds().y());
  }

  void TestWindowOrder(const std::vector<aura::Window*>& window_order) {
    Launcher* launcher = Launcher::ForPrimaryDisplay();
    int panel_index = model_->FirstPanelIndex();
    EXPECT_EQ((int)(panel_index + window_order.size()), model_->item_count());
    for (std::vector<aura::Window*>::const_iterator iter =
         window_order.begin(); iter != window_order.end();
         ++iter, ++panel_index) {
      LauncherID id = launcher->delegate()->GetIDByWindow(*iter);
      EXPECT_EQ(id, model_->items()[panel_index].id);
    }
  }

  // Test dragging panel window along the shelf and verify that panel icons
  // are reordered appropriately.
  void DragAlongShelfReorder(int dx, int dy) {
    gfx::Rect bounds(0, 0, 201, 201);
    scoped_ptr<aura::Window> w1(CreatePanelWindow(bounds));
    scoped_ptr<aura::Window> w2(CreatePanelWindow(bounds));
    std::vector<aura::Window*> window_order_original;
    std::vector<aura::Window*> window_order_swapped;
    window_order_original.push_back(w1.get());
    window_order_original.push_back(w2.get());
    window_order_swapped.push_back(w2.get());
    window_order_swapped.push_back(w1.get());
    TestWindowOrder(window_order_original);

    // Drag window #2 to the beginning of the shelf.
    DragStart(w2.get());
    DragMove(400 * dx, 400 * dy);
    DragEnd();

    // Expect swapped window order.
    TestWindowOrder(window_order_swapped);

    // Drag window #2 back to the end.
    DragStart(w2.get());
    DragMove(-400 * dx, -400 * dy);
    DragEnd();

    // Expect original order.
    TestWindowOrder(window_order_original);
  }

 private:
  scoped_ptr<PanelWindowResizer> resizer_;
  aura::Window* panel_container_;
  internal::PanelLayoutManager* panel_layout_manager_;
  gfx::Rect launcher_bounds_;
  LauncherModel* model_;

  DISALLOW_COPY_AND_ASSIGN(PanelWindowResizerTest);
};

// Verifies a window can be dragged from the panel and detached and then
// reattached.
TEST_F(PanelWindowResizerTest, PanelDetachReattachBottom) {
  DetachReattachTest(0, -1);
}

TEST_F(PanelWindowResizerTest, PanelDetachReattachLeft) {
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_LEFT, shell->GetPrimaryRootWindow());
  DetachReattachTest(1, 0);
}

#if defined(OS_WIN)
// TODO(flackr): Positioning of the panel seems to be off on Windows Aura when
// attached to the right (http://crbug.com/180892).
#define MAYBE_PanelDetachReattachRight DISABLED_PanelDetachReattachRight
#else
#define MAYBE_PanelDetachReattachRight PanelDetachReattachRight
#endif

TEST_F(PanelWindowResizerTest, MAYBE_PanelDetachReattachRight) {
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_RIGHT,
                           shell->GetPrimaryRootWindow());
  DetachReattachTest(-1, 0);
}

TEST_F(PanelWindowResizerTest, PanelDetachReattachTop) {
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_TOP, shell->GetPrimaryRootWindow());
  DetachReattachTest(0, 1);
}

TEST_F(PanelWindowResizerTest, DragReordersPanelsHorizontal) {
  DragAlongShelfReorder(-1, 0);
}

TEST_F(PanelWindowResizerTest, DragReordersPanelsVertical) {
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->SetShelfAlignment(SHELF_ALIGNMENT_LEFT, shell->GetPrimaryRootWindow());
  DragAlongShelfReorder(0, -1);
}

}  // namespace internal
}  // namespace ash
