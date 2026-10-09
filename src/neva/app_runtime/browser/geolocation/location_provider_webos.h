// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_RUNTIME_BROWSER_GEOLOCATION_LOCATION_PROVIDER_WEBOS_H_
#define NEVA_APP_RUNTIME_BROWSER_GEOLOCATION_LOCATION_PROVIDER_WEBOS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/geoposition.mojom.h"

namespace neva_app_runtime {

// Positions for the Geolocation API from com.webos.service.location, the
// system location service the rest of webOS uses: GPS, network and mock
// positions, switched on and off in Settings.
//
// The provider lives on the geolocation sequence. The luna subscription lives
// on the UI thread, because the luna client attaches to the default GLib main
// context, which only the UI thread runs; its results are posted back.
class LocationProviderWebos : public device::LocationProvider {
 public:
  LocationProviderWebos();
  LocationProviderWebos(const LocationProviderWebos&) = delete;
  LocationProviderWebos& operator=(const LocationProviderWebos&) = delete;
  ~LocationProviderWebos() override;

  // device::LocationProvider:
  void FillDiagnostics(
      device::mojom::GeolocationDiagnostics& diagnostics) override;
  void SetUpdateCallback(
      const LocationProviderUpdateCallback& callback) override;
  void StartProvider(bool high_accuracy) override;
  void StopProvider() override;
  const device::mojom::GeopositionResult* GetPosition() override;
  void OnPermissionGranted() override;

 private:
  class Subscription;

  void UpdateSubscription();
  void OnResult(device::mojom::GeopositionResultPtr result);

  SEQUENCE_CHECKER(sequence_checker_);

  LocationProviderUpdateCallback callback_;
  device::mojom::GeopositionResultPtr last_result_;
  bool started_ = false;
  bool permission_granted_ = false;
  bool subscribed_ = false;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  std::unique_ptr<Subscription, base::OnTaskRunnerDeleter> subscription_;

  base::WeakPtrFactory<LocationProviderWebos> weak_factory_{this};
};

}  // namespace neva_app_runtime

#endif  // NEVA_APP_RUNTIME_BROWSER_GEOLOCATION_LOCATION_PROVIDER_WEBOS_H_
