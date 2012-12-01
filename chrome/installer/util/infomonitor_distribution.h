// Copyright (c) 2012 Cemron Ltd. All rights reserved.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_INFOMONITOR_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_INFOMONITOR_DISTRIBUTION_H_

#include <string>

#include "chrome/installer/util/browser_distribution.h"

class InfomonitorDistribution : public BrowserDistribution {
 public:
  virtual string16 GetAppGuid() OVERRIDE;

  virtual string16 GetBaseAppName() OVERRIDE;

  virtual string16 GetAppShortCutName() OVERRIDE;

  virtual string16 GetAlternateApplicationName() OVERRIDE;

  virtual string16 GetBaseAppId() OVERRIDE;

  virtual string16 GetInstallSubDir() OVERRIDE;

  virtual string16 GetPublisherName() OVERRIDE;

  virtual string16 GetAppDescription() OVERRIDE;

  virtual string16 GetLongAppDescription() OVERRIDE;

  virtual std::string GetSafeBrowsingName() OVERRIDE;

  virtual string16 GetStateKey() OVERRIDE;

  virtual string16 GetStateMediumKey() OVERRIDE;

  virtual string16 GetUninstallLinkName() OVERRIDE;

  virtual string16 GetUninstallRegPath() OVERRIDE;

  virtual string16 GetVersionKey() OVERRIDE;

  virtual bool CanSetAsDefault() OVERRIDE;

  virtual int GetIconIndex() OVERRIDE;

  virtual bool GetChromeChannel(string16* channel) OVERRIDE;

  virtual bool GetCommandExecuteImplClsid(
      string16* handler_class_uuid) OVERRIDE;

 protected:
  friend class BrowserDistribution;

  InfomonitorDistribution();

 private:
  DISALLOW_COPY_AND_ASSIGN(InfomonitorDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_INFOMONITOR_DISTRIBUTION_H_
