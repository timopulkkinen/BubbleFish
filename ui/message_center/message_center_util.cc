// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_util.h"

#include "base/command_line.h"
#include "ui/message_center/message_center_switches.h"

namespace message_center {

bool IsRichNotificationEnabled() {
#if defined(OS_WIN) && defined(USE_AURA)
  return false;
#else
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableRichNotifications);
#endif
}

}  // namespace message_center
