// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/in_memory_tab_restore_service.h"

#include "base/compiler_specific.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"

InMemoryTabRestoreService::InMemoryTabRestoreService(
    Profile* profile,
    TabRestoreService::TimeFactory* time_factory)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        helper_(this, NULL, profile, time_factory)) {
}

InMemoryTabRestoreService::~InMemoryTabRestoreService() {}

void InMemoryTabRestoreService::AddObserver(
    TabRestoreServiceObserver* observer) {
  helper_.AddObserver(observer);
}

void InMemoryTabRestoreService::RemoveObserver(
    TabRestoreServiceObserver* observer) {
  helper_.RemoveObserver(observer);
}

void InMemoryTabRestoreService::CreateHistoricalTab(
    content::WebContents* contents,
    int index) {
  helper_.CreateHistoricalTab(contents, index);
}

void InMemoryTabRestoreService::BrowserClosing(
    TabRestoreServiceDelegate* delegate) {
  helper_.BrowserClosing(delegate);
}

void InMemoryTabRestoreService::BrowserClosed(
    TabRestoreServiceDelegate* delegate) {
  helper_.BrowserClosed(delegate);
}

void InMemoryTabRestoreService::ClearEntries() {
  helper_.ClearEntries();
}

const TabRestoreService::Entries& InMemoryTabRestoreService::entries() const {
  return helper_.entries();
}

void InMemoryTabRestoreService::RestoreMostRecentEntry(
    TabRestoreServiceDelegate* delegate,
    chrome::HostDesktopType host_desktop_type) {
  helper_.RestoreMostRecentEntry(delegate, host_desktop_type);
}

TabRestoreService::Tab* InMemoryTabRestoreService::RemoveTabEntryById(
    SessionID::id_type id) {
  return helper_.RemoveTabEntryById(id);
}

void InMemoryTabRestoreService::RestoreEntryById(
    TabRestoreServiceDelegate* delegate,
    SessionID::id_type id,
    chrome::HostDesktopType host_desktop_type,
    WindowOpenDisposition disposition) {
  helper_.RestoreEntryById(delegate, id, host_desktop_type, disposition);
}

void InMemoryTabRestoreService::LoadTabsFromLastSession() {
  // Do nothing. This relies on tab persistence which is implemented in Java on
  // the application side on Android.
}

bool InMemoryTabRestoreService::IsLoaded() const {
  // See comment above.
  return true;
}

void InMemoryTabRestoreService::DeleteLastSession() {
  // See comment above.
}

void InMemoryTabRestoreService::Shutdown() {
}

ProfileKeyedService* TabRestoreServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new InMemoryTabRestoreService(profile, NULL);
}
