// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/webos/palm_audio_class.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Every class SoundPlayer::play() accepted plays on the matching palm sink.
TEST(PalmAudioClassTest, KnownClassesMapToTheirSink) {
  EXPECT_EQ(PalmAudioClassToSink("ringtones"), "pringtones");
  EXPECT_EQ(PalmAudioClassToSink("alarm"), "palarm");
  EXPECT_EQ(PalmAudioClassToSink("alerts"), "palerts");
  EXPECT_EQ(PalmAudioClassToSink("notifications"), "pnotifications");
  EXPECT_EQ(PalmAudioClassToSink("calendar"), "pcalendar");
  EXPECT_EQ(PalmAudioClassToSink("media"), "pmedia");
  EXPECT_EQ(PalmAudioClassToSink("voicedial"), "pvoicedial");
  EXPECT_EQ(PalmAudioClassToSink("flash"), "pflash");
  EXPECT_EQ(PalmAudioClassToSink("navigation"), "pnavigation");
  EXPECT_EQ(PalmAudioClassToSink("defaultapp"), "pdefaultapp");
  EXPECT_EQ(PalmAudioClassToSink("feedback"), "pfeedback");
}

// Anything else keeps the element on the device it had.
TEST(PalmAudioClassTest, UnknownClassesMapToNothing) {
  EXPECT_TRUE(PalmAudioClassToSink("").empty());
  EXPECT_TRUE(PalmAudioClassToSink("Alarm").empty());
  EXPECT_TRUE(PalmAudioClassToSink("alarm ").empty());
  EXPECT_TRUE(PalmAudioClassToSink("palarm").empty());
  EXPECT_TRUE(PalmAudioClassToSink("tts").empty());
  EXPECT_TRUE(PalmAudioClassToSink("default").empty());
  EXPECT_TRUE(PalmAudioClassToDeviceId("voip").empty());
}

TEST(PalmAudioClassTest, DeviceIdRoundTrips) {
  for (const auto& [audio_class, sink] : kPalmAudioClassSinks) {
    const std::string device_id = PalmAudioClassToDeviceId(audio_class);
    EXPECT_EQ(device_id, std::string("palm-audio-class:") + std::string(sink));
    EXPECT_TRUE(IsPalmAudioClassDeviceId(device_id));
    EXPECT_EQ(PalmAudioClassDeviceIdToSink(device_id), sink);
  }
}

// Only the reserved prefix followed by a known sink is a class device id, so
// it cannot be used to open a stream on an arbitrary PulseAudio sink.
TEST(PalmAudioClassTest, OtherDeviceIdsAreNotClassDeviceIds) {
  EXPECT_FALSE(IsPalmAudioClassDeviceId(""));
  EXPECT_FALSE(IsPalmAudioClassDeviceId("default"));
  EXPECT_FALSE(IsPalmAudioClassDeviceId("default1"));
  EXPECT_FALSE(IsPalmAudioClassDeviceId("palarm"));
  EXPECT_FALSE(IsPalmAudioClassDeviceId("palm-audio-class:"));
  EXPECT_FALSE(IsPalmAudioClassDeviceId("palm-audio-class:alarm"));
  EXPECT_FALSE(IsPalmAudioClassDeviceId("palm-audio-class:pcm_output"));
  EXPECT_FALSE(IsPalmAudioClassDeviceId("palm-audio-class:palarmx"));
  EXPECT_FALSE(IsPalmAudioClassDeviceId("palm-audio-class:palar"));
  EXPECT_FALSE(IsPalmAudioClassDeviceId("Palm-audio-class:palarm"));
  EXPECT_FALSE(IsPalmAudioClassDeviceId("xpalm-audio-class:palarm"));
}

}  // namespace media
