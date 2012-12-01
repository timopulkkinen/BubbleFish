// Copyright (c) 2012 Cemron Ltd. All rights reserved.
//
// This file declares a class that contains various method related to branding.
// This file is not currently used

#include "chrome/installer/util/infomonitor_distribution.h"

#include "base/logging.h"

namespace {

	// remove this if used
	

const wchar_t kCommandExecuteImplUuid[] =
    L"{A2DF06F9-A21A-44A8-8A99-8B9C84F29161}";
const wchar_t kInfomonitorGuid[] = L"{FDA71E6F-AC4C-4a00-8B70-9958A68906FF}";

}  // namespace

InfomonitorDistribution::InfomonitorDistribution()
    : BrowserDistribution(CHROME_BROWSER /* should be INFOMONITOR_PLAYER */ ) {

}
string16 InfomonitorDistribution::GetAppGuid() {
  return kInfomonitorGuid;
}

string16 InfomonitorDistribution::GetBaseAppName() {
  return L"Infomonitor Player";
}

string16 InfomonitorDistribution::GetAppShortCutName() {
  return GetBaseAppName();
}

string16 InfomonitorDistribution::GetAlternateApplicationName() {
  return L"The Infomonitor Player";
}

string16 InfomonitorDistribution::GetBaseAppId() {
  return L"InfomonitorPlayer";
}

string16 InfomonitorDistribution::GetInstallSubDir() {
  return L"InfomonitorPlayer";
}

string16 InfomonitorDistribution::GetPublisherName() {
  return L"InfomonitorPlayer";
}

string16 InfomonitorDistribution::GetAppDescription() {
  return L"Cemron Infomonitor Player";
}

string16 InfomonitorDistribution::GetLongAppDescription() {
  return L"Cemron Infomonitor Player";
}

std::string InfomonitorDistribution::GetSafeBrowsingName() {
  return "infomonitorplayer";
}

string16 InfomonitorDistribution::GetStateKey() {
  return L"Software\\Infomonitor Player";
}

string16 InfomonitorDistribution::GetStateMediumKey() {
  return L"Software\\Infomonitor Player";
}

string16 InfomonitorDistribution::GetUninstallLinkName() {
  return L"Uninstall Infomonitor Player";
}

string16 InfomonitorDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Infomonitor Player";
}

string16 InfomonitorDistribution::GetVersionKey() {
  return L"Software\\Infomonitor Player";
}

bool InfomonitorDistribution::CanSetAsDefault() {
  return false;
}


int InfomonitorDistribution::GetIconIndex() {
  return 0;
}

bool InfomonitorDistribution::GetChromeChannel(string16* channel) {
  return false;
}

bool InfomonitorDistribution::GetCommandExecuteImplClsid(
    string16* handler_class_uuid) {
  if (handler_class_uuid)
    *handler_class_uuid = kCommandExecuteImplUuid;
  return true;
}