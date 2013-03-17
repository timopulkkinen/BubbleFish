// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_MOCK_LOCATION_ARBITRATOR_H_
#define CONTENT_BROWSER_GEOLOCATION_MOCK_LOCATION_ARBITRATOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/browser/geolocation/location_arbitrator.h"

namespace content {

struct Geoposition;

class MockGeolocationArbitrator : public GeolocationArbitrator {
 public:
  MockGeolocationArbitrator();

  bool providers_started() const { return providers_started_; }

  // GeolocationArbitrator:
  virtual void StartProviders(const GeolocationObserverOptions& options)
      OVERRIDE;
  virtual void StopProviders() OVERRIDE;
  virtual void OnPermissionGranted() OVERRIDE;
  virtual bool HasPermissionBeenGranted() const OVERRIDE;

 private:
  bool permission_granted_;
  bool providers_started_;

  DISALLOW_COPY_AND_ASSIGN(MockGeolocationArbitrator);
};

}  // namespace content

#endif  //  CONTENT_BROWSER_GEOLOCATION_MOCK_LOCATION_ARBITRATOR_H_
