// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_RESOURCE_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_RESOURCE_METADATA_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/chromeos/drive/drive_file_error.h"

namespace base {
class SequencedTaskRunner;
}

namespace drive {

class DriveDirectoryProto;
class DriveEntryProto;
class DriveResourceMetadataStorage;

typedef std::vector<DriveEntryProto> DriveEntryProtoVector;
typedef std::map<std::string /* resource_id */, DriveEntryProto>
    DriveEntryProtoMap;

// Holds information needed to fetch contents of a directory.
// This object is copyable.
class DirectoryFetchInfo {
 public:
  DirectoryFetchInfo() : changestamp_(0) {}
  DirectoryFetchInfo(const std::string& resource_id,
                     int64 changestamp)
      : resource_id_(resource_id),
        changestamp_(changestamp) {
  }

  // Returns true if the object is empty.
  bool empty() const { return resource_id_.empty(); }

  // Resource ID of the directory.
  const std::string& resource_id() const { return resource_id_; }

  // Changestamp of the directory. The changestamp is used to determine if
  // the directory contents should be fetched.
  int64 changestamp() const { return changestamp_; }

 private:
  const std::string resource_id_;
  const int64 changestamp_;
};

// File type on the drive file system can be either a regular file or
// a hosted document.
enum DriveFileType {
  REGULAR_FILE,
  HOSTED_DOCUMENT,
};

// The root directory name used for the Google Drive file system tree. The
// name is used in URLs for the file manager, hence user-visible.
const base::FilePath::CharType kDriveRootDirectory[] =
    FILE_PATH_LITERAL("drive");

// This should be incremented when incompatibility change is made in
// drive.proto.
const int32 kProtoVersion = 2;

// Callback similar to FileOperationCallback but with a given |file_path|.
// Used for operations that change a file path like moving files.
typedef base::Callback<void(DriveFileError error,
                            const base::FilePath& file_path)>
    FileMoveCallback;

// Used to get entry info from the file system.
// If |error| is not DRIVE_FILE_OK, |entry_info| is set to NULL.
typedef base::Callback<void(DriveFileError error,
                            scoped_ptr<DriveEntryProto> entry_proto)>
    GetEntryInfoCallback;

typedef base::Callback<void(DriveFileError error,
                            scoped_ptr<DriveEntryProtoVector> entries)>
    ReadDirectoryCallback;

// Used to get entry info from the file system, with the Drive file path.
// If |error| is not DRIVE_FILE_OK, |entry_proto| is set to NULL.
//
// |drive_file_path| parameter is provided as DriveEntryProto does not contain
// the Drive file path (i.e. only contains the base name without parent
// directory names).
typedef base::Callback<void(DriveFileError error,
                            const base::FilePath& drive_file_path,
                            scoped_ptr<DriveEntryProto> entry_proto)>
    GetEntryInfoWithFilePathCallback;

// Used to get a set of changed directories for feed processing.
typedef base::Callback<void(const std::set<base::FilePath>&)>
    GetChildDirectoriesCallback;

typedef base::Callback<void(int64)> GetChangestampCallback;

// This is a part of EntryInfoPairResult.
struct EntryInfoResult {
  EntryInfoResult();
  ~EntryInfoResult();

  base::FilePath path;
  DriveFileError error;
  scoped_ptr<DriveEntryProto> proto;
};

// The result of GetEntryInfoPairCallback(). Used to get a pair of entries
// in one function call.
struct EntryInfoPairResult {
  EntryInfoPairResult();
  ~EntryInfoPairResult();

  EntryInfoResult first;
  EntryInfoResult second;  // Only filled if the first entry is found.
};

// Used to receive the result from GetEntryInfoPairCallback().
typedef base::Callback<void(scoped_ptr<EntryInfoPairResult> pair_result)>
    GetEntryInfoPairCallback;

// Storage for Drive Metadata.
class DriveResourceMetadata {
 public:
  // |root_resource_id| is the resource id for the root directory.
  explicit DriveResourceMetadata(const std::string& root_resource_id);
  virtual ~DriveResourceMetadata();

  // Last time when we dumped serialized file system to disk.
  const base::Time& last_serialized() const { return last_serialized_; }
  void set_last_serialized(const base::Time& time) { last_serialized_ = time; }

  // Size of serialized file system on disk in bytes.
  size_t serialized_size() const { return serialized_size_; }
  void set_serialized_size(size_t size) { serialized_size_ = size; }

  // True if the file system feed is loaded from the cache or from the server.
  bool loaded() const { return loaded_; }
  void set_loaded(bool loaded) { loaded_ = loaded; }

  // Largest change timestamp that was the source of content for the current
  // state of the root directory.
  void GetLargestChangestamp(const GetChangestampCallback& callback);
  void SetLargestChangestamp(int64 value,
                             const FileOperationCallback& callback);

  // Adds |entry_proto| to the metadata tree, based on its parent_resource_id.
  // |callback| must not be null.
  void AddEntry(const DriveEntryProto& entry_proto,
                const FileMoveCallback& callback);

  // Moves entry specified by |file_path| to the directory specified by
  // |directory_path| and calls the callback asynchronously. Removes the entry
  // from the previous parent. |callback| must not be null.
  void MoveEntryToDirectory(const base::FilePath& file_path,
                            const base::FilePath& directory_path,
                            const FileMoveCallback& callback);

  // Renames entry specified by |file_path| with the new name |new_name| and
  // calls |callback| asynchronously. |callback| must not be null.
  void RenameEntry(const base::FilePath& file_path,
                   const base::FilePath::StringType& new_name,
                   const FileMoveCallback& callback);

  // Removes entry with |resource_id| from its parent. Calls |callback| with the
  // path of the parent directory. |callback| must not be null.
  void RemoveEntry(const std::string& resource_id,
                   const FileMoveCallback& callback);

  // Finds an entry (a file or a directory) by |resource_id|.
  // |callback| must not be null.
  void GetEntryInfoByResourceId(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback);

  // Finds an entry (a file or a directory) by |file_path|.
  // |callback| must not be null.
  void GetEntryInfoByPath(const base::FilePath& file_path,
                          const GetEntryInfoCallback& callback);

  // Finds and reads a directory by |file_path|.
  // |callback| must not be null.
  void ReadDirectoryByPath(const base::FilePath& file_path,
                           const ReadDirectoryCallback& callback);

  // Similar to GetEntryInfoByPath() but this function finds a pair of
  // entries by |first_path| and |second_path|. If the entry for
  // |first_path| is not found, this function does not try to get the
  // entry of |second_path|. |callback| must not be null.
  void GetEntryInfoPairByPaths(const base::FilePath& first_path,
                               const base::FilePath& second_path,
                               const GetEntryInfoPairCallback& callback);

  // Refreshes a drive entry with the same resource id as |entry_proto|.
  // |callback| is run with the error, file path and proto of the entry.
  // |callback| must not be null.
  void RefreshEntry(const DriveEntryProto& entry_proto,
                    const GetEntryInfoWithFilePathCallback& callback);

  // Removes all child files of the directory pointed by
  // |directory_fetch_info| and replaces them with
  // |entry_proto_map|. The changestamp of the directory will be updated per
  // |directory_fetch_info|. |callback| is called with the directory path.
  // |callback| must not be null.
  //
  // TODO(satorux): For "fast fetch" crbug.com/178348, this function should
  // be able to update child directories too. The existing directories should
  // remain as-is, but the new directories should be added with changestamp
  // set to zero, which will be fast fetched.
  void RefreshDirectory(const DirectoryFetchInfo& directory_fetch_info,
                        const DriveEntryProtoMap& entry_proto_map,
                        const FileMoveCallback& callback);

  // Recursively get child directories of entry pointed to by |resource_id|.
  void GetChildDirectories(
      const std::string& resource_id,
      const GetChildDirectoriesCallback& changed_dirs_callback);

  // Removes all files/directories under root (not including root).
  void RemoveAll(const base::Closure& callback);

  // Serializes/Parses to/from string via proto classes.
  void SerializeToString(std::string* serialized_proto);
  bool ParseFromString(const std::string& serialized_proto);

 private:
  // Clears root_ and the resource map.
  void ClearRoot();

  // Continues with GetEntryInfoPairByPaths after the first DriveEntry has been
  // asynchronously fetched. This fetches the second DriveEntry only if the
  // first was found.
  void GetEntryInfoPairByPathsAfterGetFirst(
      const base::FilePath& first_path,
      const base::FilePath& second_path,
      const GetEntryInfoPairCallback& callback,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Continues with GetIntroInfoPairByPaths after the second DriveEntry has been
  // asynchronously fetched.
  void GetEntryInfoPairByPathsAfterGetSecond(
      const base::FilePath& second_path,
      const GetEntryInfoPairCallback& callback,
      scoped_ptr<EntryInfoPairResult> result,
      DriveFileError error,
      scoped_ptr<DriveEntryProto> entry_proto);

  // Searches for |file_path| synchronously.
  scoped_ptr<DriveEntryProto> FindEntryByPathSync(
      const base::FilePath& file_path);

  // Helper function to get a directory given |resource_id|. |resource_id| can
  // not be empty. Returns NULL if it finds no corresponding entry, or the
  // corresponding entry is not a directory.
  scoped_ptr<DriveEntryProto> GetDirectory(const std::string& resource_id);

  // Returns virtual file path of the entry.
  base::FilePath GetFilePath(const std::string& resource_id);

  // Recursively extracts the paths set of all sub-directories.
  void GetDescendantDirectoryPaths(const std::string& resource_id,
                                   std::set<base::FilePath>* child_directories);

  // Adds a child entry to its parent directory.
  // The method will also do name de-duplication to ensure that the
  // exposed presentation path does not have naming conflicts. Two files with
  // the same name "Foo" will be renames to "Foo (1)" and "Foo (2)".
  void AddEntryToDirectory(const DriveEntryProto& entry);

  // Removes the entry from its parent directory.
  void RemoveDirectoryChild(const std::string& child_resource_id);

  // Detaches the entry from its parent directory.
  void DetachEntryFromDirectory(const std::string& child_resource_id);

  // Removes child elements of directory.
  void RemoveDirectoryChildren(const std::string& directory_resource_id);

  // Adds directory children recursively from |proto|.
  void AddDescendantsFromProto(const DriveDirectoryProto& proto);

  // Converts directory to proto.
  void DirectoryToProto(const std::string& directory_resource_id,
                        DriveDirectoryProto* proto);

  // Converts the children as a vector of DriveEntryProto.
  scoped_ptr<DriveEntryProtoVector> DirectoryChildrenToProtoVector(
      const std::string& directory_resource_id);

  // Private data members.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  scoped_ptr<DriveResourceMetadataStorage> storage_;

  std::string root_resource_id_;

  base::Time last_serialized_;
  size_t serialized_size_;
  int64 largest_changestamp_;  // Stored in the serialized proto.
  bool loaded_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<DriveResourceMetadata> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveResourceMetadata);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_RESOURCE_METADATA_H_
