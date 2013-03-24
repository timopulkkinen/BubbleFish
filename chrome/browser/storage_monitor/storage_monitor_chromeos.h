// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chromeos::StorageMonitorCros listens for mount point changes and notifies
// listeners about the addition and deletion of media devices.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_CHROMEOS_H_
#define CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_CHROMEOS_H_

#if !defined(OS_CHROMEOS)
#error "Should only be used on ChromeOS."
#endif

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chromeos/disks/disk_mount_manager.h"

namespace chrome {
class MediaTransferProtocolDeviceObserverLinux;
}

namespace chromeos {

class StorageMonitorCros
    : public chrome::StorageMonitor,
      public base::RefCountedThreadSafe<StorageMonitorCros>,
      public disks::DiskMountManager::Observer {
 public:
  // Should only be called by browser start up code. Use GetInstance() instead.
  StorageMonitorCros();

  // Sets up disk listeners and issues notifications for any discovered
  // mount points. Sets up MTP manager and listeners.
  void Init();

  virtual void OnDiskEvent(disks::DiskMountManager::DiskEvent event,
                           const disks::DiskMountManager::Disk* disk) OVERRIDE;
  virtual void OnDeviceEvent(disks::DiskMountManager::DeviceEvent event,
                             const std::string& device_path) OVERRIDE;
  virtual void OnMountEvent(
      disks::DiskMountManager::MountEvent event,
      MountError error_code,
      const disks::DiskMountManager::MountPointInfo& mount_info) OVERRIDE;
  virtual void OnFormatEvent(disks::DiskMountManager::FormatEvent event,
                             FormatError error_code,
                             const std::string& device_path) OVERRIDE;

  // Finds the device that contains |path| and populates |device_info|.
  // Returns false if unable to find the device.
  virtual bool GetStorageInfoForPath(
      const base::FilePath& path,
      chrome::StorageInfo* device_info) const OVERRIDE;

  // Returns the storage size of the device present at |location|. If the
  // device information is unavailable, returns zero.
  virtual uint64 GetStorageSize(const std::string& location) const OVERRIDE;

  virtual void EjectDevice(
      const std::string& device_id,
      base::Callback<void(EjectStatus)> callback) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<StorageMonitorCros>;

  // Mapping of mount path to removable mass storage info.
  typedef std::map<std::string, chrome::StorageInfo> MountMap;

  // Private to avoid code deleting the object.
  virtual ~StorageMonitorCros();

  // Checks existing mount points map for media devices. For each mount point,
  // call CheckMountedPathOnFileThread() below.
  void CheckExistingMountPointsOnUIThread();

  // Checks if the mount point in |mount_info| is a media device. If it is,
  // then continue with AddMountedPathOnUIThread() below.
  void CheckMountedPathOnFileThread(
      const disks::DiskMountManager::MountPointInfo& mount_info);

  // Adds the mount point in |mount_info| to |mount_map_| and send a media
  // device attach notification. |has_dcim| is true if the attached device has
  // a DCIM folder.
  void AddMountedPathOnUIThread(
      const disks::DiskMountManager::MountPointInfo& mount_info,
      bool has_dcim);

  // Mapping of relevant mount points and their corresponding mount devices.
  // Only accessed on the UI thread.
  MountMap mount_map_;

  scoped_ptr<chrome::MediaTransferProtocolDeviceObserverLinux>
      media_transfer_protocol_device_observer_;

  DISALLOW_COPY_AND_ASSIGN(StorageMonitorCros);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_CHROMEOS_H_
