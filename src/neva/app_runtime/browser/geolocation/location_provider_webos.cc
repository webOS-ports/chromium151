// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_runtime/browser/geolocation/location_provider_webos.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "neva/pal_service/luna/luna_client.h"
#include "neva/pal_service/luna/luna_names.h"

namespace neva_app_runtime {

namespace {

const char kClientName[] = "com.webos.chromium.location";
const char kGetLocationUpdates[] =
    "luna://com.webos.service.location/getLocationUpdates";
// No "Handler": the service then uses its hybrid handler, GPS and network
// together, whichever of them Settings has switched on.
const char kSubscribeParams[] = "{\"subscribe\":true}";

// How long to wait before asking again after the service answered with an
// error, which ends the subscription: location may have been switched on in
// Settings since, or the service restarted.
constexpr base::TimeDelta kRetryDelay = base::Seconds(10);

// com.webos.service.location's LocationErrorCode values that need telling
// apart here (include/ServiceAgent.h).
constexpr int kLocationSuccess = 0;
constexpr int kLocationTimeOut = 1;
constexpr int kLocationLocationOff = 5;
constexpr int kLocationAppBlackListed = 7;

device::mojom::GeopositionResultPtr MakeError(
    device::mojom::GeopositionErrorCode code,
    const std::string& message,
    const std::string& technical) {
  return device::mojom::GeopositionResult::NewError(
      device::mojom::GeopositionError::New(code, message, technical));
}

std::optional<double> FindNumber(const base::DictValue& dict, const char* key) {
  if (const base::Value* value = dict.Find(key)) {
    if (value->is_double() || value->is_int()) {
      return value->GetDouble();
    }
  }
  return std::nullopt;
}

// Turns one reply of getLocationUpdates into a result, or nothing for a reply
// that carries neither a position nor an error (the subscription's first,
// "subscribed" acknowledgement).
device::mojom::GeopositionResultPtr ParseReply(const std::string& payload) {
  std::optional<base::DictValue> reply =
      base::JSONReader::ReadDict(payload, base::JSON_PARSE_RFC);
  if (!reply) {
    return MakeError(device::mojom::GeopositionErrorCode::kPositionUnavailable,
                     "Location service sent a reply that is not JSON", payload);
  }

  std::optional<int> error_code = reply->FindInt("errorCode");
  std::optional<bool> return_value = reply->FindBool("returnValue");
  if ((error_code && *error_code != kLocationSuccess) ||
      (return_value && !*return_value)) {
    const int code = error_code.value_or(-1);
    const std::string* text = reply->FindString("errorText");
    const std::string technical = "com.webos.service.location errorCode " +
                                  std::to_string(code) +
                                  (text ? ": " + *text : std::string());

    switch (code) {
      case kLocationLocationOff:
        return MakeError(
            device::mojom::GeopositionErrorCode::kPositionUnavailable,
            "Location services are switched off in Settings", technical);
      case kLocationAppBlackListed:
        return MakeError(device::mojom::GeopositionErrorCode::kPermissionDenied,
                         "Location is blocked for this application", technical);
      case kLocationTimeOut:
      default:
        return MakeError(
            device::mojom::GeopositionErrorCode::kPositionUnavailable,
            text ? *text : "Position unavailable", technical);
    }
  }

  std::optional<double> latitude = FindNumber(*reply, "latitude");
  std::optional<double> longitude = FindNumber(*reply, "longitude");
  if (!latitude || !longitude) {
    return nullptr;
  }

  auto position = device::mojom::Geoposition::New();
  position->latitude = *latitude;
  position->longitude = *longitude;

  // The Geolocation API needs an accuracy. The service leaves it out, or
  // negative, when the handler had none to give; report that as unknown but
  // large rather than as perfectly precise.
  std::optional<double> accuracy = FindNumber(*reply, "horizAccuracy");
  position->accuracy = (accuracy && *accuracy >= 0) ? *accuracy : 10000.0;

  if (std::optional<double> altitude = FindNumber(*reply, "altitude")) {
    position->altitude = *altitude;
  }
  std::optional<double> vert_accuracy = FindNumber(*reply, "vertAccuracy");
  if (vert_accuracy && *vert_accuracy >= 0) {
    position->altitude_accuracy = *vert_accuracy;
  }
  std::optional<double> speed = FindNumber(*reply, "speed");
  if (speed && *speed >= 0) {
    position->speed = *speed;
    // A heading only means something while moving.
    std::optional<double> direction = FindNumber(*reply, "direction");
    if (*speed > 0 && direction && *direction >= 0 && *direction < 360) {
      position->heading = *direction;
    }
  }

  // Milliseconds since the epoch, as the service sends them.
  std::optional<double> timestamp = FindNumber(*reply, "timestamp");
  position->timestamp =
      (timestamp && *timestamp > 0)
          ? base::Time::FromMillisecondsSinceUnixEpoch(*timestamp)
          : base::Time::Now();

  return device::mojom::GeopositionResult::NewPosition(std::move(position));
}

}  // namespace

// The luna side. Created, used and destroyed on the UI thread.
class LocationProviderWebos::Subscription {
 public:
  using ResultCallback =
      base::RepeatingCallback<void(device::mojom::GeopositionResultPtr)>;

  explicit Subscription(ResultCallback callback)
      : callback_(std::move(callback)) {}
  Subscription(const Subscription&) = delete;
  Subscription& operator=(const Subscription&) = delete;
  ~Subscription() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    Stop();
  }

  void Start() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    active_ = true;
    if (token_) {
      return;
    }

    if (!client_) {
      pal::luna::Client::Params params;
      params.name = pal::luna::GetServiceNameWithRandSuffix(kClientName);
      client_ = pal::luna::CreateClient(params);
    }
    if (!client_ || !client_->IsInitialized() ||
        !client_->Subscribe(kGetLocationUpdates, kSubscribeParams,
                            base::BindRepeating(&Subscription::OnReply,
                                                weak_factory_.GetWeakPtr()),
                            std::string(), &token_)) {
      LOG(ERROR) << "Cannot subscribe to " << kGetLocationUpdates;
      token_ = 0;
      callback_.Run(
          MakeError(device::mojom::GeopositionErrorCode::kPositionUnavailable,
                    "Location service is not reachable", kGetLocationUpdates));
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&Subscription::Retry, weak_factory_.GetWeakPtr()),
          kRetryDelay);
    }
  }

  void Stop() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    active_ = false;
    Unsubscribe();
  }

 private:
  void OnReply(pal::luna::Client::ResponseStatus status,
               unsigned token,
               const std::string& payload) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (status == pal::luna::Client::ResponseStatus::CANCELED) {
      return;
    }
    device::mojom::GeopositionResultPtr result = ParseReply(payload);
    if (!result) {
      return;
    }
    const bool failed = result->is_error();
    callback_.Run(std::move(result));
    if (failed && active_) {
      // The error ended the subscription; ask again later.
      Unsubscribe();
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&Subscription::Retry, weak_factory_.GetWeakPtr()),
          kRetryDelay);
    }
  }

  void Retry() {
    if (active_) {
      Start();
    }
  }

  void Unsubscribe() {
    if (client_ && token_) {
      client_->Unsubscribe(token_);
    }
    token_ = 0;
  }

  ResultCallback callback_;
  std::unique_ptr<pal::luna::Client> client_;
  unsigned token_ = 0;
  // Whether the provider wants positions, as opposed to whether a
  // subscription is open right now.
  bool active_ = false;
  base::WeakPtrFactory<Subscription> weak_factory_{this};
};

LocationProviderWebos::LocationProviderWebos()
    : ui_task_runner_(content::GetUIThreadTaskRunner({})),
      subscription_(nullptr, base::OnTaskRunnerDeleter(ui_task_runner_)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

LocationProviderWebos::~LocationProviderWebos() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // subscription_ is deleted on the UI thread, which also unsubscribes.
}

void LocationProviderWebos::FillDiagnostics(
    device::mojom::GeolocationDiagnostics& diagnostics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!started_) {
    diagnostics.provider_state =
        device::mojom::GeolocationDiagnostics::ProviderState::kStopped;
  } else {
    diagnostics.provider_state =
        device::mojom::GeolocationDiagnostics::ProviderState::kLowAccuracy;
  }
}

void LocationProviderWebos::SetUpdateCallback(
    const LocationProviderUpdateCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = callback;
}

void LocationProviderWebos::StartProvider(bool high_accuracy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // high_accuracy is not passed on: the hybrid handler already uses GPS when
  // Settings has it on.
  started_ = true;
  UpdateSubscription();
}

void LocationProviderWebos::StopProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  started_ = false;
  UpdateSubscription();
}

const device::mojom::GeopositionResult* LocationProviderWebos::GetPosition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return last_result_.get();
}

void LocationProviderWebos::OnPermissionGranted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  permission_granted_ = true;
  UpdateSubscription();
}

// Subscribes while the provider is started and the page has permission, and
// only then: a provider may be started before the user has answered.
void LocationProviderWebos::UpdateSubscription() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool want = started_ && permission_granted_;
  if (want == subscribed_) {
    return;
  }
  subscribed_ = want;

  if (!subscription_) {
    subscription_.reset(new Subscription(
        base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                           base::BindRepeating(&LocationProviderWebos::OnResult,
                                               weak_factory_.GetWeakPtr()))));
  }

  // Unretained: subscription_ is deleted by a task posted to the same runner
  // after these, so it outlives them.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(want ? &Subscription::Start : &Subscription::Stop,
                     base::Unretained(subscription_.get())));
}

void LocationProviderWebos::OnResult(
    device::mojom::GeopositionResultPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!subscribed_) {
    return;
  }
  last_result_ = result.Clone();
  if (callback_) {
    callback_.Run(this, std::move(result));
  }
}

}  // namespace neva_app_runtime
