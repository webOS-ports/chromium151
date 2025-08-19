// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NEVA_APP_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define NEVA_APP_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include "content/public/browser/speech_recognition_event_listener.h"
#include "build/build_config.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"

namespace extensions {
namespace speech {

class ShellSpeechRecognitionManagerDelegate
    : public content::SpeechRecognitionManagerDelegate,
      public content::SpeechRecognitionEventListener {
 public:
  ShellSpeechRecognitionManagerDelegate();

  ShellSpeechRecognitionManagerDelegate(
      const ShellSpeechRecognitionManagerDelegate&) = delete;
  ShellSpeechRecognitionManagerDelegate& operator=(
      const ShellSpeechRecognitionManagerDelegate&) = delete;

  ~ShellSpeechRecognitionManagerDelegate() override;

 private:
  // SpeechRecognitionEventListener methods.
  void OnRecognitionStart(int session_id) override;
  void OnAudioStart(int session_id) override;
  void OnSoundStart(int session_id) override;
  void OnSoundEnd(int session_id) override;
  void OnAudioEnd(int session_id) override;
  void OnRecognitionEnd(int session_id) override;
  void OnRecognitionResults(
      int session_id,
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results)
      override;
  void OnRecognitionError(
      int session_id,
      const media::mojom::SpeechRecognitionError& error) override;
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override;

  // SpeechRecognitionManagerDelegate methods.
  void CheckRecognitionIsAllowed(
      int session_id,
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback)
      override;
  content::SpeechRecognitionEventListener* GetEventListener() override;
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
  // app_shell has no on-device speech service; leave the receiver unbound.
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver,
      const std::string& language) override {}
#endif

  static void CheckRenderFrameType(
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback,
      int render_process_id,
      int render_frame_id);
};

}  // namespace speech
}  // namespace extensions

#endif  // NEVA_APP_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
