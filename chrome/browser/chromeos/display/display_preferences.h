// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_

#include "base/basictypes.h"

class PrefRegistrySimple;

namespace gfx {
class Display;
class Insets;
}

namespace chromeos {

// Registers the prefs associated with display settings and stored
// into Local State.
void RegisterDisplayLocalStatePrefs(PrefRegistrySimple* registry);

// Stores the current displays prefereces (both primary display id and
// dispay layout).
void StoreDisplayPrefs();

// Sets the display layout for the current displays and store them.
void SetAndStoreDisplayLayoutPref(int layout, int offset);

// Stores the display layout for given display pairs.
void StoreDisplayLayoutPref(int64 id1, int64 id2, int layout, int offset);

// Sets and stores the primary display device by its ID, and notifies
// the update to the system.
void SetAndStorePrimaryDisplayIDPref(int64 display_id);

// Sets and saves the overscan preference for the specified |display| to Local
// State.
void SetAndStoreDisplayOverscan(const gfx::Display& display,
                                const gfx::Insets& insets);

// Checks the current display settings in Local State and notifies them to the
// system.
void NotifyDisplayLocalStatePrefChanged();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_PREFERENCES_H_
