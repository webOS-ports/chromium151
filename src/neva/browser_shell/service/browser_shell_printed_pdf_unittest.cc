// Copyright 2026 Herman van Hazendonk <github.com@herrie.org>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "neva/browser_shell/service/browser_shell_printed_pdf.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_shell {

namespace {

constexpr char kPdf[] = "%PDF-1.4\n%fake\n";

base::span<const uint8_t> Bytes(const char* text) {
  return base::as_byte_span(std::string_view(text));
}

int CountFiles(const base::FilePath& dir) {
  int n = 0;
  base::FileEnumerator files(dir, false, base::FileEnumerator::FILES);
  for (base::FilePath p = files.Next(); !p.empty(); p = files.Next()) {
    ++n;
  }
  return n;
}

class PrintedPdfTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_.CreateUniqueTempDir());
    dir_ = temp_.GetPath().Append("Print");
    now_ = base::Time::Now();
  }

  base::FilePath Write(base::span<const uint8_t> data, std::string* error) {
    return WritePrintedPdf(dir_, data, now_, kPrintedPdfMaxAge, error);
  }

  base::ScopedTempDir temp_;
  base::FilePath dir_;
  base::Time now_;
};

}  // namespace

TEST_F(PrintedPdfTest, WritesTheBytesForItsOwnerOnly) {
  std::string error;
  const base::FilePath path = Write(Bytes(kPdf), &error);
  ASSERT_FALSE(path.empty()) << error;
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(dir_, path.DirName());
  EXPECT_EQ(FILE_PATH_LITERAL(".pdf"), path.Extension());

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path, &contents));
  EXPECT_EQ(kPdf, contents);

  int mode = 0;
  ASSERT_TRUE(base::GetPosixFilePermissions(path, &mode));
  EXPECT_EQ(0600, mode);
  ASSERT_TRUE(base::GetPosixFilePermissions(dir_, &mode));
  EXPECT_EQ(0700, mode);
  // Nothing is left of the temporary file it was written to.
  EXPECT_EQ(1, CountFiles(dir_));
}

TEST_F(PrintedPdfTest, NarrowsAFolderThatIsThere) {
  ASSERT_TRUE(base::CreateDirectory(dir_));
  ASSERT_TRUE(base::SetPosixFilePermissions(dir_, 0755));
  std::string error;
  ASSERT_FALSE(Write(Bytes(kPdf), &error).empty()) << error;
  int mode = 0;
  ASSERT_TRUE(base::GetPosixFilePermissions(dir_, &mode));
  EXPECT_EQ(0700, mode);
}

TEST_F(PrintedPdfTest, EachPrintHasAFileOfItsOwn) {
  std::string error;
  // The same millisecond, as two cards printing at once would be.
  const base::FilePath first = Write(Bytes(kPdf), &error);
  const base::FilePath second = Write(Bytes("%PDF-1.4\n2\n"), &error);
  ASSERT_FALSE(first.empty());
  ASSERT_FALSE(second.empty());
  EXPECT_NE(first, second);

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(first, &contents));
  EXPECT_EQ(kPdf, contents);
  EXPECT_EQ(2, CountFiles(dir_));
}

TEST_F(PrintedPdfTest, RemovesThePdfsOlderThanTheirAge) {
  ASSERT_TRUE(base::CreateDirectory(dir_));
  const base::FilePath old_pdf = dir_.Append("old.pdf");
  const base::FilePath recent_pdf = dir_.Append("recent.pdf");
  const base::FilePath old_other = dir_.Append("old.txt");
  for (const auto& p : {old_pdf, recent_pdf, old_other}) {
    ASSERT_TRUE(base::WriteFile(p, "x"));
  }
  const base::Time long_ago = now_ - kPrintedPdfMaxAge - base::Minutes(1);
  ASSERT_TRUE(base::TouchFile(old_pdf, long_ago, long_ago));
  ASSERT_TRUE(base::TouchFile(old_other, long_ago, long_ago));
  const base::Time lately = now_ - base::Minutes(5);
  ASSERT_TRUE(base::TouchFile(recent_pdf, lately, lately));

  std::string error;
  const base::FilePath path = Write(Bytes(kPdf), &error);
  ASSERT_FALSE(path.empty()) << error;
  EXPECT_FALSE(base::PathExists(old_pdf));
  EXPECT_TRUE(base::PathExists(recent_pdf));
  // Only PDFs are the engine's to remove.
  EXPECT_TRUE(base::PathExists(old_other));
  EXPECT_TRUE(base::PathExists(path));
}

TEST_F(PrintedPdfTest, RefusesAFolderThatIsALink) {
  const base::FilePath elsewhere = temp_.GetPath().Append("elsewhere");
  ASSERT_TRUE(base::CreateDirectory(elsewhere));
  const base::FilePath victim = elsewhere.Append("keep.pdf");
  ASSERT_TRUE(base::WriteFile(victim, "x"));
  const base::Time long_ago = now_ - base::Days(2);
  ASSERT_TRUE(base::TouchFile(victim, long_ago, long_ago));
  ASSERT_TRUE(base::CreateSymbolicLink(elsewhere, dir_));

  std::string error;
  EXPECT_TRUE(Write(Bytes(kPdf), &error).empty());
  EXPECT_FALSE(error.empty());
  // Neither written to nor cleaned out through the link.
  EXPECT_TRUE(base::PathExists(victim));
  EXPECT_EQ(1, CountFiles(elsewhere));
}

TEST_F(PrintedPdfTest, RefusesNoDataAndNoFolder) {
  std::string error;
  EXPECT_TRUE(Write(base::span<const uint8_t>(), &error).empty());
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(base::PathExists(dir_));

  error.clear();
  EXPECT_TRUE(WritePrintedPdf(base::FilePath(FILE_PATH_LITERAL("Print")),
                              Bytes(kPdf), now_, kPrintedPdfMaxAge, &error)
                  .empty());
  EXPECT_FALSE(error.empty());

  error.clear();
  EXPECT_TRUE(WritePrintedPdf(base::FilePath(), Bytes(kPdf), now_,
                              kPrintedPdfMaxAge, &error)
                  .empty());
  EXPECT_FALSE(error.empty());
}

}  // namespace browser_shell
