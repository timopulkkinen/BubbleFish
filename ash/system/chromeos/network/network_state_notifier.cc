// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/network/network_state_notifier.h"

#include "ash/shell.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/ash_strings.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const char kLogModule[] = "NetworkStateNotifier";

ash::NetworkObserver::NetworkType GetAshNetworkType(const std::string& type) {
  if (type == flimflam::kTypeCellular)
    return ash::NetworkObserver::NETWORK_CELLULAR;
  if (type == flimflam::kTypeEthernet)
     return ash::NetworkObserver::NETWORK_ETHERNET;
  if (type == flimflam::kTypeWifi)
     return ash::NetworkObserver::NETWORK_WIFI;
  if (type == flimflam::kTypeBluetooth)
     return ash::NetworkObserver::NETWORK_BLUETOOTH;
  return ash::NetworkObserver::NETWORK_UNKNOWN;
}

string16 GetErrorString(const std::string& error) {
  if (error == flimflam::kErrorOutOfRange)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_OUT_OF_RANGE);
  if (error == flimflam::kErrorPinMissing)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_PIN_MISSING);
  if (error == flimflam::kErrorDhcpFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_DHCP_FAILED);
  if (error == flimflam::kErrorConnectFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
  if (error == flimflam::kErrorBadPassphrase)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_BAD_PASSPHRASE);
  if (error == flimflam::kErrorBadWEPKey)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_BAD_WEPKEY);
  if (error == flimflam::kErrorActivationFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
  }
  if (error == flimflam::kErrorNeedEvdo)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_NEED_EVDO);
  if (error == flimflam::kErrorNeedHomeNetwork) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_NEED_HOME_NETWORK);
  }
  if (error == flimflam::kErrorOtaspFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_OTASP_FAILED);
  if (error == flimflam::kErrorAaaFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_AAA_FAILED);
  if (error == flimflam::kErrorInternal)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_INTERNAL);
  if (error == flimflam::kErrorDNSLookupFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_DNS_LOOKUP_FAILED);
  }
  if (error == flimflam::kErrorHTTPGetFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_HTTP_GET_FAILED);
  }
  if (error == flimflam::kErrorIpsecPskAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_IPSEC_PSK_AUTH_FAILED);
  }
  if (error == flimflam::kErrorIpsecCertAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_IPSEC_CERT_AUTH_FAILED);
  }
  if (error == flimflam::kErrorPppAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_PPP_AUTH_FAILED);
  }
  if (error == flimflam::kUnknownString)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
  return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
}

}  // namespace

using chromeos::NetworkState;
using chromeos::NetworkStateHandler;

namespace ash {
namespace internal {

NetworkStateNotifier::NetworkStateNotifier() {
  if (NetworkStateHandler::Get()) {
    NetworkStateHandler::Get()->AddObserver(this);
    InitializeNetworks();
  }
}

NetworkStateNotifier::~NetworkStateNotifier() {
  if (NetworkStateHandler::Get())
    NetworkStateHandler::Get()->RemoveObserver(this);
}

void NetworkStateNotifier::NetworkConnectionStateChanged(
    const NetworkState* network) {
  NetworkStateHandler* handler = NetworkStateHandler::Get();
  std::string prev_state;
  std::string new_state = network->connection_state();
  CachedStateMap::iterator iter = cached_state_.find(network->path());
  if (iter != cached_state_.end()) {
    prev_state = iter->second;
    if (prev_state == new_state)
      return;  // No state change
    VLOG(1) << "NetworkStateNotifier: State: " << prev_state
            << " ->: " << new_state;
    iter->second = new_state;
  } else {
    VLOG(1) << "NetworkStateNotifier: New Service: " << network->path()
            << " State: " << new_state;
    cached_state_[network->path()] = new_state;
    return;  // New network, no state change
  }
  bool notify_failure = false;
  if (new_state == flimflam::kStateFailure &&
      prev_state != flimflam::kStateIdle) {
    // Note: Idle -> Failure sometimes happens on resume when the network
    // device is not ready yet, but is not an actual failure.
    notify_failure = true;
  } else if (new_state == flimflam::kStateIdle &&
             NetworkState::StateIsConnecting(prev_state) &&
             network->path() == handler->connecting_network()) {
    // Connecting -> Idle without an error shouldn't happen but sometimes does.
    notify_failure = true;
  }
  if (!notify_failure)
    return;

  chromeos::network_event_log::AddEntry(
      kLogModule, "ConnectionFailure", network->path());

  std::vector<string16> no_links;
  ash::NetworkObserver::NetworkType network_type =
      GetAshNetworkType(network->type());
  string16 error = GetErrorString(network->error());
  ash::Shell::GetInstance()->system_tray_notifier()->NotifySetNetworkMessage(
      NULL, ash::NetworkObserver::ERROR_CONNECT_FAILED, network_type,
      l10n_util::GetStringUTF16(IDS_NETWORK_CONNECTION_ERROR_TITLE),
      l10n_util::GetStringFUTF16(
            IDS_NETWORK_CONNECTION_ERROR_MESSAGE_WITH_DETAILS,
            UTF8ToUTF16(network->name()), error),
      no_links);
}

void NetworkStateNotifier::InitializeNetworks() {
  NetworkStateList network_list;
  NetworkStateHandler::Get()->GetNetworkList(&network_list);
  VLOG(1) << "NetworkStateNotifier:InitializeNetworks: "
          << network_list.size();
  for (NetworkStateList::iterator iter = network_list.begin();
       iter != network_list.end(); ++iter) {
    const NetworkState* network = *iter;
    VLOG(2) << " Network: " << network->path();
    cached_state_[network->path()] = network->connection_state();
  }
}

}  // namespace internal
}  // namespace ash
