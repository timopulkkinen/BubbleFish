// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
#define ASH_SYSTEM_TRAY_TRAY_VIEWS_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_types.h"
#include "ui/gfx/font.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"

typedef unsigned int SkColor;

namespace gfx {
class ImageSkia;
}

namespace views {
class Label;
}

namespace ash {
namespace internal {

class TrayItemView;

// A border for label buttons that paints a vertical separator in normal state
// and a custom hover effect in hovered or pressed state.
class TrayPopupLabelButtonBorder : public views::LabelButtonBorder {
 public:
  TrayPopupLabelButtonBorder();
  virtual ~TrayPopupLabelButtonBorder();

  // Overridden from views::LabelButtonBorder.
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TrayPopupLabelButtonBorder);
};

// A label button with custom alignment, border and focus border.
class TrayPopupLabelButton : public views::LabelButton {
 public:
  TrayPopupLabelButton(views::ButtonListener* listener, const string16& text);
  virtual ~TrayPopupLabelButton();

 private:
  // Overridden from views::LabelButton.
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupLabelButton);
};

// A ToggleImageButton with fixed size, paddings and hover effects. These
// buttons are used in the header.
class TrayPopupHeaderButton : public views::ToggleImageButton {
 public:
  TrayPopupHeaderButton(views::ButtonListener* listener,
                        int enabled_resource_id,
                        int disabled_resource_id,
                        int enabled_resource_id_hover,
                        int disabled_resource_id_hover,
                        int accessible_name_id);
  virtual ~TrayPopupHeaderButton();

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaintBorder(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::CustomButton.
  virtual void StateChanged() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupHeaderButton);
};

// Sets up a Label properly for the tray (sets color, font etc.).
void SetupLabelForTray(views::Label* label);

// TODO(jennyz): refactor these two functions to SystemTrayItem.
// Sets the empty border of an image tray item for adjusting the space
// around it.
void SetTrayImageItemBorder(views::View* tray_view, ShelfAlignment alignment);
// Sets the empty border around a label tray item for adjusting the space
// around it.
void SetTrayLabelItemBorder(TrayItemView* tray_view,
                            ShelfAlignment alignment);

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_VIEWS_H_
