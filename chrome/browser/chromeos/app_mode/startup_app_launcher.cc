// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"

#include "ash/shell.h"
#include "base/time.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_update_service.h"
#include "chrome/browser/chromeos/ui/app_launch_view.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/webstore_startup_installer.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::WebstoreStartupInstaller;

namespace chromeos {

namespace {

// Application install splash screen minimum show time in milliseconds.
const int kAppInstallSplashScreenMinTimeMS = 3000;

bool IsAppInstalled(Profile* profile, const std::string& app_id) {
  return extensions::ExtensionSystem::Get(profile)->extension_service()->
      GetInstalledExtension(app_id);
}

}  // namespace

StartupAppLauncher::StartupAppLauncher(Profile* profile,
                                       const std::string& app_id)
    : profile_(profile),
      app_id_(app_id),
      launch_splash_start_time_(0) {
  DCHECK(profile_);
  DCHECK(Extension::IdIsValid(app_id_));
  DCHECK(ash::Shell::HasInstance());
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

StartupAppLauncher::~StartupAppLauncher() {
  DCHECK(ash::Shell::HasInstance());
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

void StartupAppLauncher::Start() {
  launch_splash_start_time_ = base::TimeTicks::Now().ToInternalValue();
  DVLOG(1) << "Starting... connection = "
           <<  net::NetworkChangeNotifier::GetConnectionType();
  chromeos::ShowAppLaunchSplashScreen();

  // Set a maximum allowed wait time for network.
  const int kMaxNetworkWaitSeconds = 5 * 60;
  network_wait_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kMaxNetworkWaitSeconds),
      this, &StartupAppLauncher::OnNetworkWaitTimedout);

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  OnNetworkChanged(net::NetworkChangeNotifier::GetConnectionType());
}

void StartupAppLauncher::OnLaunchSuccess() {
  const int64 time_taken_ms = (base::TimeTicks::Now() -
      base::TimeTicks::FromInternalValue(launch_splash_start_time_)).
      InMilliseconds();

  // Enforce that we show app install splash screen for some minimum amount
  // of time.
  if (time_taken_ms < kAppInstallSplashScreenMinTimeMS) {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&StartupAppLauncher::OnLaunchSuccess, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            kAppInstallSplashScreenMinTimeMS - time_taken_ms));
    return;
  }

  chromeos::CloseAppLaunchSplashScreen();
  delete this;
}

void StartupAppLauncher::OnLaunchFailure() {
  // Ends the session if launch fails. This should bring us back to login.
  chrome::AttemptUserExit();

  // TODO(xiyuan): Signal somehow to the session manager that app launch
  // attempt had failed.

  delete this;
}

void StartupAppLauncher::Launch() {
  const Extension* extension = extensions::ExtensionSystem::Get(profile_)->
      extension_service()->GetInstalledExtension(app_id_);
  CHECK(extension);

  // Set the app_id for the current instance of KioskAppUpdateService.
  KioskAppUpdateService* update_service =
      KioskAppUpdateServiceFactory::GetForProfile(profile_);
  DCHECK(update_service);
  if (update_service)
    update_service->set_app_id(app_id_);

  // Always open the app in a window.
  chrome::OpenApplication(chrome::AppLaunchParams(profile_,
                                                  extension,
                                                  extension_misc::LAUNCH_WINDOW,
                                                  NEW_WINDOW));
  OnLaunchSuccess();
}

void StartupAppLauncher::BeginInstall() {
  DVLOG(1) << "BeginInstall... connection = "
           <<  net::NetworkChangeNotifier::GetConnectionType();

  chromeos::UpdateAppLaunchSplashScreenState(
      chromeos::APP_LAUNCH_STATE_INSTALLING_APPLICATION);
  if (IsAppInstalled(profile_, app_id_)) {
    Launch();
    return;
  }

  installer_ = new WebstoreStartupInstaller(
      app_id_,
      profile_,
      false,
      base::Bind(&StartupAppLauncher::InstallCallback, AsWeakPtr()));
  installer_->BeginInstall();
}

void StartupAppLauncher::InstallCallback(bool success,
                                         const std::string& error) {
  if (success) {
    // Schedules Launch() to be called after the callback returns.
    // So that the app finishes its installation.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&StartupAppLauncher::Launch, AsWeakPtr()));
    return;
  }

  LOG(ERROR) << "Failed to install app with error: " << error;
  OnLaunchFailure();
}

void StartupAppLauncher::OnNetworkWaitTimedout() {
  LOG(WARNING) << "OnNetworkWaitTimedout... connection = "
               <<  net::NetworkChangeNotifier::GetConnectionType();
  // Timeout in waiting for online. Try the install anyway.
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  BeginInstall();
}

void StartupAppLauncher::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DVLOG(1) << "OnNetworkChanged... connection = "
           <<  net::NetworkChangeNotifier::GetConnectionType();
  if (!net::NetworkChangeNotifier::IsOffline()) {
    DVLOG(1) << "Network up and running!";
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
    network_wait_timer_.Stop();

    BeginInstall();
  } else {
    DVLOG(1) << "Network not running yet!";
  }
}

void StartupAppLauncher::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::ET_KEY_PRESSED)
    return;

  if (event->key_code() != ui::VKEY_X ||
      !(event->flags() & ui::EF_CONTROL_DOWN) ||
      !(event->flags() & ui::EF_ALT_DOWN) ||
      !(event->flags() & ui::EF_SHIFT_DOWN)) {
    return;
  }

  KioskAppManager::Get()->SetSuppressAutoLaunch(true);
  OnLaunchFailure();  // User cancel failure.
}

}   // namespace chromeos
