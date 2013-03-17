// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace {

// See drive.proto for the difference between the two URLs.
const char kResumableEditMediaUrl[] = "http://resumable-edit-media/";
const char kResumableCreateMediaUrl[] = "http://resumable-create-media/";

const char kTestRootResourceId[] = "test_root";

// The changestamp of the resource metadata used in
// DriveResourceMetadataTest.
const int64 kTestChangestamp = 100;

// Copies result from GetChildDirectoriesCallback.
void CopyResultFromGetChildDirectoriesCallback(
    std::set<base::FilePath>* out_child_directories,
    const std::set<base::FilePath>& in_child_directories) {
  *out_child_directories = in_child_directories;
}

// Copies result from GetChangestampCallback.
void CopyResultFromGetChangestampCallback(
    int64* out_changestamp, int64 in_changestamp) {
  *out_changestamp = in_changestamp;
}

// Returns the sorted base names from |entries|.
std::vector<std::string> GetSortedBaseNames(
    const DriveEntryProtoVector& entries) {
  std::vector<std::string> base_names;
  for (size_t i = 0; i < entries.size(); ++i)
    base_names.push_back(entries[i].base_name());
  std::sort(base_names.begin(), base_names.end());

  return base_names;
}

}  // namespace

class DriveResourceMetadataTest : public testing::Test {
 protected:
  DriveResourceMetadataTest();

  // Creates the following files/directories
  // drive/dir1/
  // drive/dir2/
  // drive/dir1/dir3/
  // drive/dir1/file4
  // drive/dir1/file5
  // drive/dir2/file6
  // drive/dir2/file7
  // drive/dir2/file8
  // drive/dir1/dir3/file9
  // drive/dir1/dir3/file10
  static void Init(DriveResourceMetadata* resource_metadata);

  // Creates a DriveEntryProto.
  static DriveEntryProto CreateDriveEntryProto(
      int sequence_id,
      bool is_directory,
      const std::string& parent_resource_id);

  // Adds a DriveEntryProto to the metadata tree. Returns true on success.
  static bool AddDriveEntryProto(DriveResourceMetadata* resource_metadata,
                                 int sequence_id,
                                 bool is_directory,
                                 const std::string& parent_resource_id);

  // Gets the entry info by path synchronously. Returns NULL on failure.
  scoped_ptr<DriveEntryProto> GetEntryInfoByPathSync(
      const base::FilePath& file_path) {
    DriveFileError error = DRIVE_FILE_OK;
    scoped_ptr<DriveEntryProto> entry_proto;
    resource_metadata_->GetEntryInfoByPath(
        file_path,
        base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                   &error, &entry_proto));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_TRUE(error == DRIVE_FILE_OK || !entry_proto);
    return entry_proto.Pass();
  }

  // Reads the directory contents by path synchronously. Returns NULL on
  // failure.
  scoped_ptr<DriveEntryProtoVector> ReadDirectoryByPathSync(
      const base::FilePath& directory_path) {
    DriveFileError error = DRIVE_FILE_OK;
    scoped_ptr<DriveEntryProtoVector> entries;
    resource_metadata_->ReadDirectoryByPath(
        directory_path,
        base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                   &error, &entries));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_TRUE(error == DRIVE_FILE_OK || !entries);
    return entries.Pass();
  }

  scoped_ptr<DriveResourceMetadata> resource_metadata_;

 private:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
};

DriveResourceMetadataTest::DriveResourceMetadataTest()
    : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  resource_metadata_.reset(new DriveResourceMetadata(kTestRootResourceId));

  Init(resource_metadata_.get());
}

// static
void DriveResourceMetadataTest::Init(DriveResourceMetadata* resource_metadata) {
  int sequence_id = 1;
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata, sequence_id++, true, kTestRootResourceId));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata, sequence_id++, true, kTestRootResourceId));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata, sequence_id++, true, "resource_id:dir1"));

  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata, sequence_id++, false, "resource_id:dir1"));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata, sequence_id++, false, "resource_id:dir1"));

  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata, sequence_id++, false, "resource_id:dir2"));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata, sequence_id++, false, "resource_id:dir2"));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata, sequence_id++, false, "resource_id:dir2"));

  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata, sequence_id++, false, "resource_id:dir3"));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata, sequence_id++, false, "resource_id:dir3"));

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  resource_metadata->SetLargestChangestamp(
      kTestChangestamp,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback,
                 &error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
}

// static
DriveEntryProto DriveResourceMetadataTest::CreateDriveEntryProto(
    int sequence_id,
    bool is_directory,
    const std::string& parent_resource_id) {
  DriveEntryProto entry_proto;
  const std::string sequence_id_str = base::IntToString(sequence_id);
  const std::string title = (is_directory ? "dir" : "file") + sequence_id_str;
  const std::string resource_id = "resource_id:" + title;
  entry_proto.set_title(title);
  entry_proto.set_resource_id(resource_id);
  entry_proto.set_parent_resource_id(parent_resource_id);

  PlatformFileInfoProto* file_info = entry_proto.mutable_file_info();
  file_info->set_is_directory(is_directory);

  if (!is_directory) {
    DriveFileSpecificInfo* file_specific_info =
        entry_proto.mutable_file_specific_info();
    file_info->set_size(sequence_id * 1024);
    file_specific_info->set_file_md5(std::string("md5:") + title);
  } else {
    DriveDirectorySpecificInfo* directory_specific_info =
        entry_proto.mutable_directory_specific_info();
    directory_specific_info->set_changestamp(kTestChangestamp);
  }
  return entry_proto;
}

bool DriveResourceMetadataTest::AddDriveEntryProto(
    DriveResourceMetadata* resource_metadata,
    int sequence_id,
    bool is_directory,
    const std::string& parent_resource_id) {
  DriveEntryProto entry_proto = CreateDriveEntryProto(sequence_id,
                                                      is_directory,
                                                      parent_resource_id);
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath drive_file_path;

  resource_metadata->AddEntry(
      entry_proto,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  return DRIVE_FILE_OK == error;
}

TEST_F(DriveResourceMetadataTest, VersionCheck) {
  // Set up the root directory.
  DriveRootDirectoryProto proto;
  DriveEntryProto* mutable_entry =
      proto.mutable_drive_directory()->mutable_drive_entry();
  mutable_entry->mutable_file_info()->set_is_directory(true);
  mutable_entry->set_resource_id(kTestRootResourceId);
  mutable_entry->set_upload_url(kResumableCreateMediaUrl);
  mutable_entry->set_title("drive");

  DriveResourceMetadata resource_metadata(kTestRootResourceId);

  std::string serialized_proto;
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));
  // This should fail as the version is empty.
  ASSERT_FALSE(resource_metadata.ParseFromString(serialized_proto));

  // Set an older version, and serialize.
  proto.set_version(kProtoVersion - 1);
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));
  // This should fail as the version is older.
  ASSERT_FALSE(resource_metadata.ParseFromString(serialized_proto));

  // Set the current version, and serialize.
  proto.set_version(kProtoVersion);
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));
  // This should succeed as the version matches the current number.
  ASSERT_TRUE(resource_metadata.ParseFromString(serialized_proto));

  // Set a newer version, and serialize.
  proto.set_version(kProtoVersion + 1);
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));
  // This should fail as the version is newer.
  ASSERT_FALSE(resource_metadata.ParseFromString(serialized_proto));
}

TEST_F(DriveResourceMetadataTest, LargestChangestamp) {
  DriveResourceMetadata resource_metadata(kTestRootResourceId);

  int64 in_changestamp = 123456;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  resource_metadata.SetLargestChangestamp(
      in_changestamp,
      base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback,
                 &error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  int64 out_changestamp = 0;
  resource_metadata.GetLargestChangestamp(
      base::Bind(&CopyResultFromGetChangestampCallback,
                 &out_changestamp));
  google_apis::test_util::RunBlockingPoolTask();
  DCHECK_EQ(in_changestamp, out_changestamp);
}

TEST_F(DriveResourceMetadataTest, GetEntryInfoByResourceId_RootDirectory) {
  DriveResourceMetadata resource_metadata(kTestRootResourceId);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // Look up the root directory by its resource ID.
  resource_metadata.GetEntryInfoByResourceId(
      kTestRootResourceId,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("drive", entry_proto->base_name());
}

TEST_F(DriveResourceMetadataTest, GetEntryInfoByResourceId) {
  // Confirm that an existing file is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata_->GetEntryInfoByResourceId(
      "resource_id:file4",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/file4"),
            drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file4", entry_proto->base_name());

  // Confirm that a non existing file is not found.
  error = DRIVE_FILE_ERROR_FAILED;
  entry_proto.reset();
  resource_metadata_->GetEntryInfoByResourceId(
      "file:non_existing",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());
}

TEST_F(DriveResourceMetadataTest, GetEntryInfoByPath) {
  // Confirm that an existing file is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata_->GetEntryInfoByPath(
      base::FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file4", entry_proto->base_name());

  // Confirm that a non existing file is not found.
  error = DRIVE_FILE_ERROR_FAILED;
  entry_proto.reset();
  resource_metadata_->GetEntryInfoByPath(
      base::FilePath::FromUTF8Unsafe("drive/dir1/non_existing"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());
}

TEST_F(DriveResourceMetadataTest, ReadDirectoryByPath) {
  // Confirm that an existing directory is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProtoVector> entries;
  resource_metadata_->ReadDirectoryByPath(
      base::FilePath::FromUTF8Unsafe("drive/dir1"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entries.get());
  ASSERT_EQ(3U, entries->size());
  // The order is not guaranteed so we should sort the base names.
  std::vector<std::string> base_names = GetSortedBaseNames(*entries);
  EXPECT_EQ("dir3", base_names[0]);
  EXPECT_EQ("file4", base_names[1]);
  EXPECT_EQ("file5", base_names[2]);

  // Confirm that a non existing directory is not found.
  error = DRIVE_FILE_ERROR_FAILED;
  entries.reset();
  resource_metadata_->ReadDirectoryByPath(
      base::FilePath::FromUTF8Unsafe("drive/non_existing"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entries.get());

  // Confirm that reading a file results in DRIVE_FILE_ERROR_NOT_A_DIRECTORY.
  error = DRIVE_FILE_ERROR_FAILED;
  entries.reset();
  resource_metadata_->ReadDirectoryByPath(
      base::FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      base::Bind(&test_util::CopyResultsFromReadDirectoryCallback,
                 &error, &entries));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_A_DIRECTORY, error);
  EXPECT_FALSE(entries.get());
}

TEST_F(DriveResourceMetadataTest, GetEntryInfoPairByPaths) {
  // Confirm that existing two files are found.
  scoped_ptr<EntryInfoPairResult> pair_result;
  resource_metadata_->GetEntryInfoPairByPaths(
      base::FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      base::FilePath::FromUTF8Unsafe("drive/dir1/file5"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoPairCallback,
                 &pair_result));
  google_apis::test_util::RunBlockingPoolTask();
  // The first entry should be found.
  EXPECT_EQ(DRIVE_FILE_OK, pair_result->first.error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/file4"),
            pair_result->first.path);
  ASSERT_TRUE(pair_result->first.proto.get());
  EXPECT_EQ("file4", pair_result->first.proto->base_name());
  // The second entry should be found.
  EXPECT_EQ(DRIVE_FILE_OK, pair_result->second.error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/file5"),
            pair_result->second.path);
  ASSERT_TRUE(pair_result->second.proto.get());
  EXPECT_EQ("file5", pair_result->second.proto->base_name());

  // Confirm that the first non existent file is not found.
  pair_result.reset();
  resource_metadata_->GetEntryInfoPairByPaths(
      base::FilePath::FromUTF8Unsafe("drive/dir1/non_existent"),
      base::FilePath::FromUTF8Unsafe("drive/dir1/file5"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoPairCallback,
                 &pair_result));
  google_apis::test_util::RunBlockingPoolTask();
  // The first entry should not be found.
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, pair_result->first.error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/non_existent"),
            pair_result->first.path);
  ASSERT_FALSE(pair_result->first.proto.get());
  // The second entry should not be found, because the first one failed.
  EXPECT_EQ(DRIVE_FILE_ERROR_FAILED, pair_result->second.error);
  EXPECT_EQ(base::FilePath(), pair_result->second.path);
  ASSERT_FALSE(pair_result->second.proto.get());

  // Confirm that the second non existent file is not found.
  pair_result.reset();
  resource_metadata_->GetEntryInfoPairByPaths(
      base::FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      base::FilePath::FromUTF8Unsafe("drive/dir1/non_existent"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoPairCallback,
                 &pair_result));
  google_apis::test_util::RunBlockingPoolTask();
  // The first entry should be found.
  EXPECT_EQ(DRIVE_FILE_OK, pair_result->first.error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/file4"),
            pair_result->first.path);
  ASSERT_TRUE(pair_result->first.proto.get());
  EXPECT_EQ("file4", pair_result->first.proto->base_name());
  // The second entry should not be found.
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, pair_result->second.error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/non_existent"),
            pair_result->second.path);
  ASSERT_FALSE(pair_result->second.proto.get());
}

TEST_F(DriveResourceMetadataTest, RemoveEntry) {
  // Make sure file9 is found.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  const std::string file9_resource_id = "resource_id:file9";
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata_->GetEntryInfoByResourceId(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/dir3/file9"),
            drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file9", entry_proto->base_name());

  // Remove file9 using RemoveEntry.
  resource_metadata_->RemoveEntry(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/dir3"), drive_file_path);

  // file9 should no longer exist.
  resource_metadata_->GetEntryInfoByResourceId(
      file9_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());

  // Look for dir3.
  const std::string dir3_resource_id = "resource_id:dir3";
  resource_metadata_->GetEntryInfoByResourceId(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/dir3"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("dir3", entry_proto->base_name());

  // Remove dir3 using RemoveEntry.
  resource_metadata_->RemoveEntry(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1"), drive_file_path);

  // dir3 should no longer exist.
  resource_metadata_->GetEntryInfoByResourceId(
      dir3_resource_id,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_FALSE(entry_proto.get());

  // Remove unknown resource_id using RemoveEntry.
  resource_metadata_->RemoveEntry(
      "foo",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);

  // Try removing root. This should fail.
  resource_metadata_->RemoveEntry(
      kTestRootResourceId,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
          &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_ACCESS_DENIED, error);
}

TEST_F(DriveResourceMetadataTest, MoveEntryToDirectory) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // Move file8 to drive/dir1.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/dir2/file8"),
      base::FilePath::FromUTF8Unsafe("drive/dir1"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/file8"),
            drive_file_path);

  // Look up the entry by its resource id and make sure it really moved.
  resource_metadata_->GetEntryInfoByResourceId(
      "resource_id:file8",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/file8"),
            drive_file_path);

  // Move non-existent file to drive/dir1. This should fail.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/dir2/file8"),
      base::FilePath::FromUTF8Unsafe("drive/dir1"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(base::FilePath(), drive_file_path);

  // Move existing file to non-existent directory. This should fail.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/dir1/file8"),
      base::FilePath::FromUTF8Unsafe("drive/dir4"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(base::FilePath(), drive_file_path);

  // Move existing file to existing file (non-directory). This should fail.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/dir1/file8"),
      base::FilePath::FromUTF8Unsafe("drive/dir1/file4"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_A_DIRECTORY, error);
  EXPECT_EQ(base::FilePath(), drive_file_path);

  // Move the file to root.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/dir1/file8"),
      base::FilePath::FromUTF8Unsafe("drive"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/file8"), drive_file_path);

  // Move the file from root.
  resource_metadata_->MoveEntryToDirectory(
      base::FilePath::FromUTF8Unsafe("drive/file8"),
      base::FilePath::FromUTF8Unsafe("drive/dir2"),
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir2/file8"),
            drive_file_path);

  // Make sure file is still ok.
  resource_metadata_->GetEntryInfoByResourceId(
      "resource_id:file8",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir2/file8"),
            drive_file_path);
}

TEST_F(DriveResourceMetadataTest, RenameEntry) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // Rename file8 to file11.
  resource_metadata_->RenameEntry(
      base::FilePath::FromUTF8Unsafe("drive/dir2/file8"),
      "file11",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir2/file11"),
            drive_file_path);

  // Lookup the file by resource id to make sure the file actually got renamed.
  resource_metadata_->GetEntryInfoByResourceId(
      "resource_id:file8",
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir2/file11"),
            drive_file_path);

  // Rename to file7 to force a duplicate name.
  resource_metadata_->RenameEntry(
      base::FilePath::FromUTF8Unsafe("drive/dir2/file11"),
      "file7",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir2/file7 (2)"),
            drive_file_path);

  // Rename to same name. This should fail.
  resource_metadata_->RenameEntry(
      base::FilePath::FromUTF8Unsafe("drive/dir2/file7 (2)"),
      "file7 (2)",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_EXISTS, error);
  EXPECT_EQ(base::FilePath(), drive_file_path);

  // Rename non-existent.
  resource_metadata_->RenameEntry(
      base::FilePath::FromUTF8Unsafe("drive/dir2/file11"),
      "file11",
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
  EXPECT_EQ(base::FilePath(), drive_file_path);
}

TEST_F(DriveResourceMetadataTest, RefreshEntry) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // Get file9.
  entry_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir1/dir3/file9"));
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file9", entry_proto->base_name());
  ASSERT_TRUE(!entry_proto->file_info().is_directory());
  EXPECT_EQ("md5:file9", entry_proto->file_specific_info().file_md5());

  // Rename it and change the file size.
  DriveEntryProto file_entry_proto(*entry_proto);
  const std::string updated_md5("md5:updated");
  file_entry_proto.mutable_file_specific_info()->set_file_md5(updated_md5);
  file_entry_proto.set_title("file100");
  entry_proto.reset();
  resource_metadata_->RefreshEntry(
      file_entry_proto,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/dir3/file100"),
            drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file100", entry_proto->base_name());
  ASSERT_TRUE(!entry_proto->file_info().is_directory());
  EXPECT_EQ(updated_md5, entry_proto->file_specific_info().file_md5());

  // Make sure we get the same thing from GetEntryInfoByPath.
  entry_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir1/dir3/file100"));
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file100", entry_proto->base_name());
  ASSERT_TRUE(!entry_proto->file_info().is_directory());
  EXPECT_EQ(updated_md5, entry_proto->file_specific_info().file_md5());

  // Get dir2.
  entry_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir2"));
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("dir2", entry_proto->base_name());
  ASSERT_TRUE(entry_proto->file_info().is_directory());

  // Change the name to dir100 and change the parent to drive/dir1/dir3.
  DriveEntryProto dir_entry_proto(*entry_proto);
  dir_entry_proto.set_title("dir100");
  dir_entry_proto.set_parent_resource_id("resource_id:dir3");
  entry_proto.reset();
  resource_metadata_->RefreshEntry(
      dir_entry_proto,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/dir3/dir100"),
            drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("dir100", entry_proto->base_name());
  EXPECT_TRUE(entry_proto->file_info().is_directory());
  EXPECT_EQ("resource_id:dir2", entry_proto->resource_id());

  // Make sure the children have moved over. Test file6.
  entry_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir1/dir3/dir100/file6"));
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file6", entry_proto->base_name());

  // Make sure dir2 no longer exists.
  entry_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir2"));
  EXPECT_FALSE(entry_proto.get());
}

// Test the special logic for RefreshEntry of root.
TEST_F(DriveResourceMetadataTest, RefreshEntry_Root) {
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // Get root.
  entry_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive"));
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("drive", entry_proto->base_name());
  ASSERT_TRUE(entry_proto->file_info().is_directory());
  EXPECT_EQ(kTestRootResourceId, entry_proto->resource_id());
  EXPECT_TRUE(entry_proto->upload_url().empty());

  // Set upload url and call RefreshEntry on root.
  DriveEntryProto dir_entry_proto(*entry_proto);
  dir_entry_proto.set_upload_url("http://root.upload.url/");
  entry_proto.reset();
  resource_metadata_->RefreshEntry(
      dir_entry_proto,
      base::Bind(&test_util::CopyResultsFromGetEntryInfoWithFilePathCallback,
                 &error, &drive_file_path, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive"), drive_file_path);
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("drive", entry_proto->base_name());
  EXPECT_TRUE(entry_proto->file_info().is_directory());
  EXPECT_EQ(kTestRootResourceId, entry_proto->resource_id());
  EXPECT_EQ("http://root.upload.url/", entry_proto->upload_url());

  // Make sure the children have moved over. Test file9.
  entry_proto.reset();
  entry_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir1/dir3/file9"));
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("file9", entry_proto->base_name());
}

TEST_F(DriveResourceMetadataTest, RefreshDirectory_EmtpyMap) {
  base::FilePath kDirectoryPath(FILE_PATH_LITERAL("drive/dir1"));
  const int64 kNewChangestamp = kTestChangestamp + 1;

  // Read the directory.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProtoVector> entries;
  entries = ReadDirectoryByPathSync(
      base::FilePath(kDirectoryPath));
  ASSERT_TRUE(entries.get());
  // "file4", "file5", "dir3" should exist in drive/dir1.
  ASSERT_EQ(3U, entries->size());
  std::vector<std::string> base_names = GetSortedBaseNames(*entries);
  EXPECT_EQ("dir3", base_names[0]);
  EXPECT_EQ("file4", base_names[1]);
  EXPECT_EQ("file5", base_names[2]);

  // Get the directory.
  scoped_ptr<DriveEntryProto> dir1_proto;
  dir1_proto = GetEntryInfoByPathSync(kDirectoryPath);
  ASSERT_TRUE(dir1_proto.get());
  // The changestamp should be initially kTestChangestamp.
  EXPECT_EQ(kTestChangestamp,
            dir1_proto->directory_specific_info().changestamp());

  // Update the directory with an empty map.
  base::FilePath file_path;
  DriveEntryProtoMap entry_map;
  resource_metadata_->RefreshDirectory(
      DirectoryFetchInfo(dir1_proto->resource_id(), kNewChangestamp),
      entry_map,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error,
                 &file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(kDirectoryPath, file_path);

  // Get the directory again.
  dir1_proto = GetEntryInfoByPathSync(kDirectoryPath);
  ASSERT_TRUE(dir1_proto.get());
  // The new changestamp should be set.
  EXPECT_EQ(kNewChangestamp,
            dir1_proto->directory_specific_info().changestamp());

  // Read the directory again.
  entries = ReadDirectoryByPathSync(
      base::FilePath(kDirectoryPath));
  ASSERT_TRUE(entries.get());
  // All entries ("file4", "file5", "dir3") should be gone now, as
  // RefreshDirectory() was called with an empty map.
  ASSERT_TRUE(entries->empty());
}

TEST_F(DriveResourceMetadataTest, RefreshDirectory_NonEmptyMap) {
  base::FilePath kDirectoryPath(FILE_PATH_LITERAL("drive/dir1"));
  const int64 kNewChangestamp = kTestChangestamp + 1;

  // Read the directory.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProtoVector> entries;
  entries = ReadDirectoryByPathSync(kDirectoryPath);
  ASSERT_TRUE(entries.get());
  // "file4", "file5", "dir3" should exist in drive/dir1.
  ASSERT_EQ(3U, entries->size());
  std::vector<std::string> base_names = GetSortedBaseNames(*entries);
  EXPECT_EQ("dir3", base_names[0]);
  EXPECT_EQ("file4", base_names[1]);
  EXPECT_EQ("file5", base_names[2]);

  // Get the directory dir1.
  scoped_ptr<DriveEntryProto> dir1_proto;
  dir1_proto = GetEntryInfoByPathSync(kDirectoryPath);
  ASSERT_TRUE(dir1_proto.get());
  // The changestamp should be initially kTestChangestamp.
  EXPECT_EQ(kTestChangestamp,
            dir1_proto->directory_specific_info().changestamp());

  // Get the directory dir2 (existing non-child directory).
  // This directory will be moved to "drive/dir1/dir2".
  scoped_ptr<DriveEntryProto> dir2_proto;
  dir2_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir2"));
  ASSERT_TRUE(dir2_proto.get());
  EXPECT_EQ(kTestChangestamp,
            dir2_proto->directory_specific_info().changestamp());
  // Change the parent resource ID, as dir2 will be moved to "drive/dir1/dir2".
  dir2_proto->set_parent_resource_id(dir1_proto->resource_id());

  // Get the directory dir3 (existing child directory).
  // This directory will remain as "drive/dir1/dir3".
  scoped_ptr<DriveEntryProto> dir3_proto;
  dir3_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir1/dir3"));
  ASSERT_TRUE(dir3_proto.get());
  EXPECT_EQ(kTestChangestamp,
            dir3_proto->directory_specific_info().changestamp());
  EXPECT_TRUE(dir3_proto->upload_url().empty());
  // Change the upload URL so we can test that the directory entry is updated
  // properly at a later time
  dir3_proto->set_upload_url("http://new_update_url/");

  // Create a map.
  DriveEntryProtoMap entry_map;

  // Add a new file to the map.
  DriveEntryProto new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  new_file.set_parent_resource_id(dir1_proto->resource_id());
  entry_map["new_file_id"] = new_file;

  // Add a new directory to the map.
  DriveEntryProto new_directory;
  new_directory.set_title("new_directory");
  new_directory.set_resource_id("new_directory_id");
  new_directory.set_parent_resource_id(dir1_proto->resource_id());
  new_directory.mutable_file_info()->set_is_directory(true);
  entry_map["new_directory_id"] = new_directory;

  // Add dir2 and dir3 as well.
  entry_map[dir2_proto->resource_id()] = *dir2_proto;
  entry_map[dir3_proto->resource_id()] = *dir3_proto;

  // Update the directory with the map.
  base::FilePath file_path;
  resource_metadata_->RefreshDirectory(
      DirectoryFetchInfo(dir1_proto->resource_id(), kNewChangestamp),
      entry_map,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error,
                 &file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(kDirectoryPath, file_path);

  // Get the directory again.
  dir1_proto = GetEntryInfoByPathSync(kDirectoryPath);
  ASSERT_TRUE(dir1_proto.get());
  // The new changestamp should be set.
  EXPECT_EQ(kNewChangestamp,
            dir1_proto->directory_specific_info().changestamp());

  // Read the directory again.
  entries = ReadDirectoryByPathSync(kDirectoryPath);
  ASSERT_TRUE(entries.get());
  // "file4", "file5", should be gone now. "dir3" should remain. "new_file"
  // "new_directory", "dir2" should now be added.
  ASSERT_EQ(4U, entries->size());
  base_names = GetSortedBaseNames(*entries);
  EXPECT_EQ("dir2", base_names[0]);
  EXPECT_EQ("dir3", base_names[1]);
  EXPECT_EQ("new_directory", base_names[2]);
  EXPECT_EQ("new_file", base_names[3]);

  // Get the new directory.
  scoped_ptr<DriveEntryProto> new_directory_proto;
  new_directory_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir1/new_directory"));
  ASSERT_TRUE(new_directory_proto.get());
  // The changestamp should be 0 for a new directory.
  EXPECT_EQ(0, new_directory_proto->directory_specific_info().changestamp());

  // Get the directory dir3 (existing child directory) again.
  dir3_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir1/dir3"));
  ASSERT_TRUE(dir3_proto.get());
  // The changestamp should not be changed.
  EXPECT_EQ(kTestChangestamp,
            dir3_proto->directory_specific_info().changestamp());
  // The new upload URL should be returned.
  EXPECT_EQ("http://new_update_url/", dir3_proto->upload_url());

  // Read the directory dir3. The contents should remain.
  // See the comment at Init() for the contents of the dir3.
  entries = ReadDirectoryByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir1/dir3"));
  ASSERT_TRUE(entries.get());
  ASSERT_EQ(2U, entries->size());

  // Get the directory dir2 (existing non-child directory) again using the
  // old path.  This should fail, as dir2 is now moved to drive/dir1/dir2.
  dir2_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir2"));
  ASSERT_FALSE(dir2_proto.get());

  // Get the directory dir2 (existing non-child directory) again using the
  // new path. This should succeed.
  dir2_proto = GetEntryInfoByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir1/dir2"));
  ASSERT_TRUE(dir2_proto.get());
  // The changestamp should not be changed.
  EXPECT_EQ(kTestChangestamp,
            dir2_proto->directory_specific_info().changestamp());

  // Read the directory dir2. The contents should remain.
  // See the comment at Init() for the contents of the dir2.
  entries = ReadDirectoryByPathSync(
      base::FilePath::FromUTF8Unsafe("drive/dir1/dir2"));
  ASSERT_TRUE(entries.get());
  ASSERT_EQ(3U, entries->size());
}

TEST_F(DriveResourceMetadataTest, RefreshDirectory_WrongParentResourceId) {
  base::FilePath kDirectoryPath(FILE_PATH_LITERAL("drive/dir1"));
  const int64 kNewChangestamp = kTestChangestamp + 1;

  // Get the directory dir1.
  scoped_ptr<DriveEntryProto> dir1_proto;
  dir1_proto = GetEntryInfoByPathSync(kDirectoryPath);
  ASSERT_TRUE(dir1_proto.get());

  // Create a map and add a new file to it.
  DriveEntryProtoMap entry_map;
  DriveEntryProto new_file;
  new_file.set_title("new_file");
  new_file.set_resource_id("new_file_id");
  // Set a random parent resource ID. This entry should not be added because
  // the parent resource ID does not match dir1_proto->resource_id().
  new_file.set_parent_resource_id("some-random-resource-id");
  entry_map["new_file_id"] = new_file;

  // Update the directory with the map.
  base::FilePath file_path;
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  resource_metadata_->RefreshDirectory(
      DirectoryFetchInfo(dir1_proto->resource_id(), kNewChangestamp),
      entry_map,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error,
                 &file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(kDirectoryPath, file_path);

  // Read the directory. Confirm that the new file is not added.
  scoped_ptr<DriveEntryProtoVector> entries;
  entries = ReadDirectoryByPathSync(kDirectoryPath);
  ASSERT_TRUE(entries.get());
  ASSERT_TRUE(entries->empty());
}

TEST_F(DriveResourceMetadataTest, AddEntry) {
  int sequence_id = 100;
  DriveEntryProto file_entry_proto = CreateDriveEntryProto(
      sequence_id++, false, "resource_id:dir3");

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath drive_file_path;

  // Add to dir3.
  resource_metadata_->AddEntry(
      file_entry_proto,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/dir3/file100"),
            drive_file_path);

  // Add a directory.
  DriveEntryProto dir_entry_proto = CreateDriveEntryProto(
      sequence_id++, true, "resource_id:dir1");

  resource_metadata_->AddEntry(
      dir_entry_proto,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/dir1/dir101"),
            drive_file_path);

  // Add to an invalid parent.
  DriveEntryProto file_entry_proto3 = CreateDriveEntryProto(
      sequence_id++, false, "resource_id:invalid");

  resource_metadata_->AddEntry(
      file_entry_proto3,
      base::Bind(&test_util::CopyResultsFromFileMoveCallback,
                 &error, &drive_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
}

TEST_F(DriveResourceMetadataTest, GetChildDirectories) {
  std::set<base::FilePath> child_directories;

  // file9: not a directory, so no children.
  resource_metadata_->GetChildDirectories("resource_id:file9",
      base::Bind(&CopyResultFromGetChildDirectoriesCallback,
                 &child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_TRUE(child_directories.empty());

  // dir2: no child directories.
  resource_metadata_->GetChildDirectories("resource_id:dir2",
      base::Bind(&CopyResultFromGetChildDirectoriesCallback,
                 &child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_TRUE(child_directories.empty());

  // dir1: dir3 is the only child
  resource_metadata_->GetChildDirectories("resource_id:dir1",
      base::Bind(&CopyResultFromGetChildDirectoriesCallback,
                 &child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(1u, child_directories.size());
  EXPECT_EQ(1u, child_directories.count(
      base::FilePath::FromUTF8Unsafe("drive/dir1/dir3")));

  // Add a few more directories to make sure deeper nesting works.
  // dir2/dir100
  // dir2/dir101
  // dir2/dir101/dir102
  // dir2/dir101/dir103
  // dir2/dir101/dir104
  // dir2/dir101/dir102/dir105
  // dir2/dir101/dir102/dir105/dir106
  // dir2/dir101/dir102/dir105/dir106/dir107
  int sequence_id = 100;
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir2"));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir2"));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir101"));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir101"));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir101"));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir102"));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir105"));
  ASSERT_TRUE(AddDriveEntryProto(
      resource_metadata_.get(), sequence_id++, true, "resource_id:dir106"));

  resource_metadata_->GetChildDirectories("resource_id:dir2",
      base::Bind(&CopyResultFromGetChildDirectoriesCallback,
                 &child_directories));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(8u, child_directories.size());
  EXPECT_EQ(1u, child_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/dir2/dir101")));
  EXPECT_EQ(1u, child_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/dir2/dir101/dir104")));
  EXPECT_EQ(1u, child_directories.count(base::FilePath::FromUTF8Unsafe(
      "drive/dir2/dir101/dir102/dir105/dir106/dir107")));
}

TEST_F(DriveResourceMetadataTest, RemoveAll) {
  // root has children.
  scoped_ptr<DriveEntryProtoVector> entries;
  entries = ReadDirectoryByPathSync(base::FilePath::FromUTF8Unsafe("drive"));
  ASSERT_TRUE(entries.get());
  ASSERT_FALSE(entries->empty());

  // remove all children.
  resource_metadata_->RemoveAll(base::Bind(&base::DoNothing));
  google_apis::test_util::RunBlockingPoolTask();

  base::FilePath drive_file_path;
  scoped_ptr<DriveEntryProto> entry_proto;

  // root should continue to exist.
  entry_proto = GetEntryInfoByPathSync(base::FilePath::FromUTF8Unsafe("drive"));
  ASSERT_TRUE(entry_proto.get());
  EXPECT_EQ("drive", entry_proto->base_name());
  ASSERT_TRUE(entry_proto->file_info().is_directory());
  EXPECT_EQ(kTestRootResourceId, entry_proto->resource_id());

  // root should have no children.
  entries = ReadDirectoryByPathSync(base::FilePath::FromUTF8Unsafe("drive"));
  ASSERT_TRUE(entries.get());
  EXPECT_TRUE(entries->empty());
}

TEST_F(DriveResourceMetadataTest, PerDirectoryChangestamp) {
  const int kNewChangestamp = kTestChangestamp + 1;
  const char kSubDirectoryResourceId[] = "sub-directory-id";

  DriveRootDirectoryProto proto;
  proto.set_version(kProtoVersion);
  proto.set_largest_changestamp(kNewChangestamp);

  // Set up the root directory.
  DriveDirectoryProto* root = proto.mutable_drive_directory();
  DriveEntryProto* root_entry = root->mutable_drive_entry();
  root_entry->mutable_file_info()->set_is_directory(true);
  root_entry->set_resource_id(kTestRootResourceId);
  root_entry->set_title("drive");
  // Add a sub directory.
  DriveDirectoryProto* directory = root->add_child_directories();
  DriveEntryProto* directory_entry = directory->mutable_drive_entry();
  directory_entry->mutable_file_info()->set_is_directory(true);
  directory_entry->set_resource_id(kSubDirectoryResourceId);
  directory_entry->set_parent_resource_id(kTestRootResourceId);
  directory_entry->set_title("directory");
  // At this point, both the root and the sub directory do not contain the
  // per-directory changestamp.

  DriveResourceMetadata resource_metadata(kTestRootResourceId);

  // Load the proto. This should propagate the largest changestamp to every
  // directory.
  std::string serialized_proto;
  ASSERT_TRUE(proto.SerializeToString(&serialized_proto));
  ASSERT_TRUE(resource_metadata.ParseFromString(serialized_proto));

  // Confirm that the root directory contains the changestamp.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  scoped_ptr<DriveEntryProto> entry_proto;
  resource_metadata.GetEntryInfoByPath(
      base::FilePath::FromUTF8Unsafe("drive"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  ASSERT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(kNewChangestamp,
            entry_proto->directory_specific_info().changestamp());

  // Confirm that the sub directory contains the changestamp.
  resource_metadata.GetEntryInfoByPath(
      base::FilePath::FromUTF8Unsafe("drive/directory"),
      base::Bind(&test_util::CopyResultsFromGetEntryInfoCallback,
                 &error, &entry_proto));
  google_apis::test_util::RunBlockingPoolTask();
  ASSERT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(kNewChangestamp,
            entry_proto->directory_specific_info().changestamp());

  // Save the current metadata to a string as serialized proto.
  std::string new_serialized_proto;
  resource_metadata.SerializeToString(&new_serialized_proto);
  // Read the proto to see if per-directory changestamps were properly saved.
  DriveRootDirectoryProto new_proto;
  ASSERT_TRUE(new_proto.ParseFromString(new_serialized_proto));

  // Confirm that the root directory contains the changestamp.
  const DriveDirectoryProto& root_proto = new_proto.drive_directory();
  EXPECT_EQ(kNewChangestamp,
            root_proto.drive_entry().directory_specific_info().changestamp());

  // Confirm that the sub directory contains the changestamp.
  ASSERT_EQ(1, new_proto.drive_directory().child_directories_size());
  const DriveDirectoryProto& dir_proto = root_proto.child_directories(0);
  EXPECT_EQ(kNewChangestamp,
            dir_proto.drive_entry().directory_specific_info().changestamp());

}

}  // namespace drive
