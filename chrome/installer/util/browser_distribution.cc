// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a class that contains various method related to branding.
// It provides only default implementations of these methods. Usually to add
// specific branding, we will need to extend this class with a custom
// implementation.

#include "chrome/installer/util/browser_distribution.h"

#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/logging.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/chrome_app_host_distribution.h"
#include "chrome/installer/util/chrome_frame_distribution.h"
#include "chrome/installer/util/chromium_binaries_distribution.h"
#include "chrome/installer/util/google_chrome_binaries_distribution.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/google_chrome_sxs_distribution.h"
#include "chrome/installer/util/infomonitor_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"

#include "installer_util_strings.h"  // NOLINT

using installer::MasterPreferences;

namespace {

	const wchar_t kCommandExecuteImplUuid[] =
    L"{A2DF06F9-A21A-44A8-8A99-8B9C84F29161}";
const wchar_t kInfomonitorGuid[] = L"{FDA71E6F-AC4C-4a00-8B70-9958A68906FF}";

// The BrowserDistribution objects are never freed.
BrowserDistribution* g_browser_distribution = NULL;
BrowserDistribution* g_chrome_frame_distribution = NULL;
BrowserDistribution* g_binaries_distribution = NULL;
BrowserDistribution* g_chrome_app_host_distribution = NULL;
BrowserDistribution* g_infomonitor_distribution = NULL;

// Returns true if currently running in npchrome_frame.dll
bool IsChromeFrameModule() {
  FilePath module_path;
  PathService::Get(base::FILE_MODULE, &module_path);
  return FilePath::CompareEqualIgnoreCase(module_path.BaseName().value(),
                                          installer::kChromeFrameDll);
}

BrowserDistribution::Type GetCurrentDistributionType() {
  // TODO(erikwright): If the app host is installed, but not Chrome, perhaps
  // this should return CHROME_APP_HOST.
  static BrowserDistribution::Type type =
      (MasterPreferences::ForCurrentProcess().install_chrome_frame() ||
       IsChromeFrameModule()) ?
          BrowserDistribution::CHROME_FRAME :
          BrowserDistribution::CHROME_BROWSER;
  return type;
}

}  // end namespace

BrowserDistribution::BrowserDistribution()
    : type_(CHROME_BROWSER) {
}

BrowserDistribution::BrowserDistribution(Type type)
    : type_(type) {
}

template<class DistributionClass>
BrowserDistribution* BrowserDistribution::GetOrCreateBrowserDistribution(
    BrowserDistribution** dist) {
  if (!*dist) {
    DistributionClass* temp = new DistributionClass();
    if (base::subtle::NoBarrier_CompareAndSwap(
            reinterpret_cast<base::subtle::AtomicWord*>(dist), NULL,
            reinterpret_cast<base::subtle::AtomicWord>(temp)) != NULL)
      delete temp;
  }

  return *dist;
}

BrowserDistribution* BrowserDistribution::GetDistribution() {
  return GetSpecificDistribution(GetCurrentDistributionType());
}

// static
BrowserDistribution* BrowserDistribution::GetSpecificDistribution(
    BrowserDistribution::Type type) {
  BrowserDistribution* dist = NULL;

  switch (type) {
    case CHROME_BROWSER:
#if defined(GOOGLE_CHROME_BUILD)
      if (InstallUtil::IsChromeSxSProcess()) {
        dist = GetOrCreateBrowserDistribution<GoogleChromeSxSDistribution>(
            &g_browser_distribution);
      } else {
        dist = GetOrCreateBrowserDistribution<GoogleChromeDistribution>(
            &g_browser_distribution);
      }
#else
      dist = GetOrCreateBrowserDistribution<BrowserDistribution>(
          &g_browser_distribution);
#endif
      break;

    case CHROME_FRAME:
      dist = GetOrCreateBrowserDistribution<ChromeFrameDistribution>(
          &g_chrome_frame_distribution);
      break;

    case CHROME_APP_HOST:
      dist = GetOrCreateBrowserDistribution<ChromeAppHostDistribution>(
          &g_chrome_app_host_distribution);
      break;

    default:
      DCHECK_EQ(CHROME_BINARIES, type);
#if defined(GOOGLE_CHROME_BUILD)
      dist = GetOrCreateBrowserDistribution<GoogleChromeBinariesDistribution>(
          &g_binaries_distribution);
#else
      dist = GetOrCreateBrowserDistribution<ChromiumBinariesDistribution>(
          &g_binaries_distribution);
#endif
  }

  return dist;
}

void BrowserDistribution::DoPostUninstallOperations(
    const Version& version, const FilePath& local_data_path,
    const string16& distribution_data) {
}

string16 BrowserDistribution::GetAppGuid() {
  return kInfomonitorGuid;
}

string16 BrowserDistribution::GetBaseAppName() {
  return L"Infomonitor Player";
}

string16 BrowserDistribution::GetAppShortCutName() {
  return GetBaseAppName();
}

string16 BrowserDistribution::GetAlternateApplicationName() {
  return L"The Infomonitor Player";
}

string16 BrowserDistribution::GetBaseAppId() {
  return L"InfomonitorPlayer";
}

string16 BrowserDistribution::GetInstallSubDir() {
  return L"InfomonitorPlayer";
}

string16 BrowserDistribution::GetPublisherName() {
  return L"Cemron Ltd";
}

string16 BrowserDistribution::GetAppDescription() {
  return L"Cemron Infomonitor Player";
}

string16 BrowserDistribution::GetLongAppDescription() {
  return L"Cemron Infomonitor Player";
}

std::string BrowserDistribution::GetSafeBrowsingName() {
  return "infomonitorplayer";
}

string16 BrowserDistribution::GetStateKey() {
  return L"Software\\Infomonitor Player";
}

string16 BrowserDistribution::GetStateMediumKey() {
  return L"Software\\Infomonitor Player";
}

string16 BrowserDistribution::GetStatsServerURL() {
  return L"";
}

std::string BrowserDistribution::GetNetworkStatsServer() const {
  return "";
}

std::string BrowserDistribution::GetHttpPipeliningTestServer() const {
  return "";
}

string16 BrowserDistribution::GetDistributionData(HKEY root_key) {
  return L"";
}

string16 BrowserDistribution::GetUninstallLinkName() {
  return L"Uninstall Infomonitor Player";
}

string16 BrowserDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Infomonitor Player";
}

string16 BrowserDistribution::GetVersionKey() {
  return L"Software\\Infomonitor Player";
}

bool BrowserDistribution::CanSetAsDefault() {
  return false;
}

bool BrowserDistribution::CanCreateDesktopShortcuts() {
  return true;
}

int BrowserDistribution::GetIconIndex() {
  return 0;
}

bool BrowserDistribution::GetChromeChannel(string16* channel) {
  return false;
}

bool BrowserDistribution::GetCommandExecuteImplClsid(
    string16* handler_class_uuid) {
  if (handler_class_uuid)
    *handler_class_uuid = kCommandExecuteImplUuid;
  return true;
}

void BrowserDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
}

bool BrowserDistribution::GetExperimentDetails(
    UserExperiment* experiment, int flavor) {
  return false;
}

void BrowserDistribution::LaunchUserExperiment(
    const FilePath& setup_path, installer::InstallStatus status,
    const Version& version, const installer::Product& product,
    bool system_level) {
}


void BrowserDistribution::InactiveUserToastExperiment(int flavor,
    const string16& experiment_group,
    const installer::Product& installation,
    const FilePath& application_path) {
}
