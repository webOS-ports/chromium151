// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WEBOS_PALM_AUDIO_CLASS_H_
#define MEDIA_AUDIO_WEBOS_PALM_AUDIO_CLASS_H_

// The legacy webOS audio classes a media element names with the
// x-palm-media-audio-class attribute, e.g. the clock app's
//   audio.setAttribute("x-palm-media-audio-class", "alarm");
// and the PulseAudio virtual sinks they play on, so that module-palm-policy
// and audiod treat the stream as that class.
//
// Blink turns a known class into a reserved output device id,
// "palm-audio-class:<sink>". The browser lets it through without the hashing
// and permission check of a real device, but only for a webOS application
// loaded by WAM; anything else gets the default device. WebOSAudioOutputStream
// opens the stream on <sink>. Such an element is played by Chromium's own
// media pipeline, not the webOS one (g-media-pipeline), which has no way to
// be told the class and plays everything on the display's default sink.
//
// Header only and free of dependencies, so Blink, the browser and the audio
// service can all use it without a new link dependency.

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace media {

inline constexpr std::string_view kPalmAudioClassAttribute =
    "x-palm-media-audio-class";

inline constexpr std::string_view kPalmAudioClassDeviceIdPrefix =
    "palm-audio-class:";

// The classes LunaSysMgr's SoundPlayer::play() accepted, and their sinks.
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 11>
    kPalmAudioClassSinks = {{
        {"ringtones", "pringtones"},
        {"alarm", "palarm"},
        {"alerts", "palerts"},
        {"notifications", "pnotifications"},
        {"calendar", "pcalendar"},
        {"media", "pmedia"},
        {"voicedial", "pvoicedial"},
        {"flash", "pflash"},
        {"navigation", "pnavigation"},
        {"defaultapp", "pdefaultapp"},
        {"feedback", "pfeedback"},
    }};

// Returns the sink of |audio_class|, or an empty view for anything that is not
// one of the classes above. Matching is exact, as it was in SoundPlayer.
constexpr std::string_view PalmAudioClassToSink(std::string_view audio_class) {
  for (const auto& [name, sink] : kPalmAudioClassSinks) {
    if (name == audio_class) {
      return sink;
    }
  }
  return {};
}

// Returns the reserved output device id for |audio_class|, or an empty string
// when the class is unknown, so the caller keeps the device it had.
inline std::string PalmAudioClassToDeviceId(std::string_view audio_class) {
  const std::string_view sink = PalmAudioClassToSink(audio_class);
  if (sink.empty()) {
    return std::string();
  }
  std::string device_id(kPalmAudioClassDeviceIdPrefix);
  device_id.append(sink);
  return device_id;
}

// Returns the sink a reserved output device id names, or an empty view when
// |device_id| is not "palm-audio-class:" followed by one of the sinks above.
constexpr std::string_view PalmAudioClassDeviceIdToSink(
    std::string_view device_id) {
  if (!device_id.starts_with(kPalmAudioClassDeviceIdPrefix)) {
    return {};
  }
  const std::string_view sink =
      device_id.substr(kPalmAudioClassDeviceIdPrefix.size());
  for (const auto& entry : kPalmAudioClassSinks) {
    if (entry.second == sink) {
      return entry.second;
    }
  }
  return {};
}

constexpr bool IsPalmAudioClassDeviceId(std::string_view device_id) {
  return !PalmAudioClassDeviceIdToSink(device_id).empty();
}

}  // namespace media

#endif  // MEDIA_AUDIO_WEBOS_PALM_AUDIO_CLASS_H_
