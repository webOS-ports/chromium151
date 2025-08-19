// Copyright (c) 2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
#include "media/base/neva/neva_mime_util_internal.h"
namespace media {
namespace internal {
void NevaMimeUtil::AddSupportedMediaFormats() {
  const CodecSet webos_codecs{VALID_CODEC};
  AddContainerWithCodecs("application/vnd.apple.mpegurl", webos_codecs);
  AddContainerWithCodecs("application/mpegurl", webos_codecs);
  AddContainerWithCodecs("application/x-mpegurl", webos_codecs);
  AddContainerWithCodecs("audio/mpegurl", webos_codecs);
  AddContainerWithCodecs("audio/x-mpegurl", webos_codecs);
#if defined(USE_NEVA_MEDIA_PLAYER_CAMERA)
  AddContainerWithCodecs("service/webos-camera", webos_codecs);
  AddContainerWithCodecs("service/webos-photo-camera", webos_codecs);
#endif  // defined(USE_NEVA_MEDIA_PLAYER_CAMERA)
}
// These containers were removed unconditionally, which is right for a TV: there the neva media
// pipeline hands decoding to fixed-function hardware that cannot do them, so claiming support would
// accept media that then fails to play. It is wrong wherever the pipeline decodes in software.
//
// USE_GST_MEDIA is exactly that distinction -- it is set when the GStreamer pipeline is in use, as on
// LuneOS, where every one of these has a decoder present (matroskademux, avdec_vp8, avdec_vp9,
// avdec_opus, vorbisdec, oggdemux, theoradec, qtdemux, avdec_aac). The WebM pair additionally honours
// the enable_webm_*_codecs GN args, which already gate the MSE side in stream_parser_factory.cc, so
// that the two paths cannot disagree about what is playable.
//
// This is what canPlayType answers from; the GN args alone do NOT reach it, which is why WebM stayed
// unsupported even with enable_webm_video_codecs=true.
void NevaMimeUtil::RemoveUnsupportedMediaFormats() {
#if !defined(ENABLE_WEBM_AUDIO_CODECS)
  RemoveContainer("audio/webm");
#endif
#if !defined(ENABLE_WEBM_VIDEO_CODECS)
  RemoveContainer("video/webm");
#endif
#if !defined(USE_GST_MEDIA)
  RemoveContainer("video/ogg");
#endif
#if BUILDFLAG(USE_PROPRIETARY_CODECS) && !defined(USE_GST_MEDIA)
  RemoveContainer("audio/aac");
  RemoveContainer("audio/x-m4a");
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS) && !defined(USE_GST_MEDIA)
}
}  // namespace internal
}  // namespace media
