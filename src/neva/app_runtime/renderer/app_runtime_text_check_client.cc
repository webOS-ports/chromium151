// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/app_runtime/renderer/app_runtime_text_check_client.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_text_checking_completion.h"
#include "third_party/blink/public/web/web_text_checking_result.h"
#include "third_party/blink/public/web/web_text_decoration_type.h"

namespace neva_app_runtime {

namespace {

// Where the keyboard's dictionaries are installed, one pair per language.
constexpr char kDictionaryDirectory[] = "/usr/share/hunspell";

// Hunspell's C API, from hunspell.h. Looked up by name, so there is nothing to
// link and no header to find in the sysroot.
using HunspellHandle = void;
using HunspellCreateFn = HunspellHandle* (*)(const char* aff, const char* dic);
using HunspellDestroyFn = void (*)(HunspellHandle*);
using HunspellSpellFn = int (*)(HunspellHandle*, const char* word);
using HunspellSuggestFn = int (*)(HunspellHandle*,
                                  char*** list,
                                  const char* word);
using HunspellFreeListFn = void (*)(HunspellHandle*, char*** list, int n);
using HunspellEncodingFn = char* (*)(HunspellHandle*);

// How many suggestions to hand back with a misspelling.
constexpr int kMaxSuggestions = 5;

// The system's Hunspell and the dictionary for the language in use, opened
// once for the process the first time anything asks. Used from Blink's main
// thread only.
class Dictionary {
 public:
  Dictionary() { Open(); }

  ~Dictionary() {
    if (handle_ && destroy_)
      destroy_(handle_);
    if (library_)
      base::UnloadNativeLibrary(library_);
  }

  bool available() const { return handle_ != nullptr; }

  bool IsCorrect(const std::string& utf8_word) const {
    return !handle_ || spell_(handle_, utf8_word.c_str()) != 0;
  }

  std::vector<std::string> Suggest(const std::string& utf8_word) const {
    std::vector<std::string> result;
    if (!handle_ || !suggest_ || !free_list_)
      return result;

    char** list = nullptr;
    const int count = suggest_(handle_, &list, utf8_word.c_str());
    for (int i = 0; i < count && i < kMaxSuggestions; ++i)
      result.emplace_back(list[i]);
    free_list_(handle_, &list, count);
    return result;
  }

 private:
  // en-US becomes en_US, then en, and last of all en_US: a device in a
  // language with no dictionary is checked as English rather than not at all.
  static std::vector<std::string> Candidates() {
    std::string locale = base::i18n::GetConfiguredLocale();
    std::replace(locale.begin(), locale.end(), '-', '_');

    std::vector<std::string> candidates;
    if (!locale.empty()) {
      candidates.push_back(locale);
      const size_t underscore = locale.find('_');
      if (underscore != std::string::npos)
        candidates.push_back(locale.substr(0, underscore));
    }
    candidates.push_back("en_US");
    return candidates;
  }

  void Open() {
    base::NativeLibraryLoadError error;
    library_ = base::LoadNativeLibrary(
        base::FilePath("libhunspell-1.7.so.0"), &error);
    if (!library_) {
      LOG(INFO) << "Spell checking is off: no Hunspell (" << error.ToString()
                << ")";
      return;
    }

    auto create = reinterpret_cast<HunspellCreateFn>(
        base::GetFunctionPointerFromNativeLibrary(library_,
                                                  "Hunspell_create"));
    destroy_ = reinterpret_cast<HunspellDestroyFn>(
        base::GetFunctionPointerFromNativeLibrary(library_,
                                                  "Hunspell_destroy"));
    spell_ = reinterpret_cast<HunspellSpellFn>(
        base::GetFunctionPointerFromNativeLibrary(library_, "Hunspell_spell"));
    suggest_ = reinterpret_cast<HunspellSuggestFn>(
        base::GetFunctionPointerFromNativeLibrary(library_,
                                                  "Hunspell_suggest"));
    free_list_ = reinterpret_cast<HunspellFreeListFn>(
        base::GetFunctionPointerFromNativeLibrary(library_,
                                                  "Hunspell_free_list"));
    auto encoding = reinterpret_cast<HunspellEncodingFn>(
        base::GetFunctionPointerFromNativeLibrary(
            library_, "Hunspell_get_dic_encoding"));

    if (!create || !destroy_ || !spell_) {
      LOG(WARNING) << "Spell checking is off: Hunspell is missing a function";
      return;
    }

    for (const std::string& language : Candidates()) {
      const base::FilePath directory(kDictionaryDirectory);
      const base::FilePath aff = directory.AppendASCII(language + ".aff");
      const base::FilePath dic = directory.AppendASCII(language + ".dic");

      if (!base::PathExists(aff) || !base::PathExists(dic))
        continue;

      HunspellHandle* handle = create(aff.value().c_str(), dic.value().c_str());
      if (!handle)
        continue;

      // Words are handed to Hunspell as UTF-8, which is what every dictionary
      // shipped here declares. One in another encoding would be fed bytes it
      // reads as something else, so it is not used.
      if (encoding) {
        const char* declared = encoding(handle);
        if (!declared || base::ToLowerASCII(std::string(declared)) != "utf-8") {
          destroy_(handle);
          continue;
        }
      }

      handle_ = handle;
      LOG(INFO) << "Spell checking with " << dic.value();
      return;
    }

    LOG(INFO) << "Spell checking is off: no dictionary for the language";
  }

  base::NativeLibrary library_ = nullptr;
  HunspellHandle* handle_ = nullptr;
  HunspellDestroyFn destroy_ = nullptr;
  HunspellSpellFn spell_ = nullptr;
  HunspellSuggestFn suggest_ = nullptr;
  HunspellFreeListFn free_list_ = nullptr;
};

const Dictionary& GetDictionary() {
  static const base::NoDestructor<Dictionary> dictionary;
  return *dictionary;
}

struct Misspelling {
  size_t offset;
  size_t length;
  std::vector<blink::WebString> suggestions;
};

// Every misspelled word in |text|, in order, up to |limit| of them (0 for all).
// Offsets and lengths are in UTF-16 code units, which is what Blink counts in.
// Suggestions are only worked out when asked for: they are not cheap.
std::vector<Misspelling> FindMisspellings(const std::u16string& text,
                                          bool with_suggestions,
                                          size_t limit) {
  std::vector<Misspelling> found;
  const Dictionary& dictionary = GetDictionary();
  if (!dictionary.available() || text.empty())
    return found;

  base::i18n::BreakIterator words(text, base::i18n::BreakIterator::BREAK_WORD);
  if (!words.Init())
    return found;

  while (words.Advance()) {
    if (!words.IsWord())
      continue;

    const std::u16string word = words.GetString();

    // Not a word to look up: a number, a code, anything with a digit in it.
    bool has_digit = false;
    for (const char16_t c : word) {
      if (c >= u'0' && c <= u'9') {
        has_digit = true;
        break;
      }
    }
    if (has_digit)
      continue;

    const std::string utf8 = base::UTF16ToUTF8(word);
    if (dictionary.IsCorrect(utf8))
      continue;

    Misspelling misspelling;
    misspelling.offset = words.prev();
    misspelling.length = word.length();

    if (with_suggestions) {
      for (const std::string& suggestion : dictionary.Suggest(utf8))
        misspelling.suggestions.push_back(blink::WebString::FromUTF8(suggestion));
    }

    found.push_back(std::move(misspelling));
    if (limit && found.size() >= limit)
      break;
  }

  return found;
}

}  // namespace

AppRuntimeTextCheckClient::AppRuntimeTextCheckClient(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  if (render_frame)
    render_frame->GetWebFrame()->SetTextCheckClient(this);
}

AppRuntimeTextCheckClient::~AppRuntimeTextCheckClient() = default;

void AppRuntimeTextCheckClient::OnDestruct() {
  delete this;
}

bool AppRuntimeTextCheckClient::IsSpellCheckingEnabled() const {
  return GetDictionary().available();
}

void AppRuntimeTextCheckClient::CheckSpelling(
    const blink::WebString& text,
    size_t& misspelled_offset,
    size_t& misspelled_length,
    std::vector<blink::WebString>* optional_suggestions) {
  misspelled_offset = 0;
  misspelled_length = 0;

  std::vector<Misspelling> found =
      FindMisspellings(text.Utf16(), optional_suggestions != nullptr, 1);
  if (found.empty())
    return;

  misspelled_offset = found.front().offset;
  misspelled_length = found.front().length;
  if (optional_suggestions)
    *optional_suggestions = std::move(found.front().suggestions);
}

void AppRuntimeTextCheckClient::RequestCheckingOfText(
    const blink::WebString& text_to_check,
    const std::vector<blink::WebSpellingMarker>& spelling_markers,
    ShouldForceRefreshTextCheckService should_force_refresh,
    std::unique_ptr<blink::WebTextCheckingCompletion> completion) {
  std::vector<blink::WebTextCheckingResult> results;

  for (Misspelling& misspelling :
       FindMisspellings(text_to_check.Utf16(), /*with_suggestions=*/true, 0)) {
    results.emplace_back(blink::kWebTextDecorationTypeSpelling,
                         static_cast<int>(misspelling.offset),
                         static_cast<int>(misspelling.length),
                         std::move(misspelling.suggestions));
  }

  completion->DidFinishCheckingText(results);
}

}  // namespace neva_app_runtime
