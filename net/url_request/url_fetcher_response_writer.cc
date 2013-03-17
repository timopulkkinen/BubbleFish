// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_fetcher_response_writer.h"

#include "base/files/file_util_proxy.h"
#include "base/single_thread_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

URLFetcherStringWriter::URLFetcherStringWriter(std::string* string)
    : string_(string) {
}

URLFetcherStringWriter::~URLFetcherStringWriter() {
}

int URLFetcherStringWriter::Initialize(const CompletionCallback& callback) {
  // Do nothing.
  return OK;
}

int URLFetcherStringWriter::Write(IOBuffer* buffer,
                                  int num_bytes,
                                  const CompletionCallback& callback) {
  string_->append(buffer->data(), num_bytes);
  return num_bytes;
}

int URLFetcherStringWriter::Finish(const CompletionCallback& callback) {
  // Do nothing.
  return OK;
}

URLFetcherFileWriter::URLFetcherFileWriter(
    scoped_refptr<base::TaskRunner> file_task_runner)
    : error_code_(OK),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      file_task_runner_(file_task_runner),
      owns_file_(false),
      file_handle_(base::kInvalidPlatformFileValue),
      total_bytes_written_(0) {
  DCHECK(file_task_runner_.get());
}

URLFetcherFileWriter::~URLFetcherFileWriter() {
  CloseAndDeleteFile();
}

int URLFetcherFileWriter::Initialize(const CompletionCallback& callback) {
  DCHECK_EQ(base::kInvalidPlatformFileValue, file_handle_);
  DCHECK(!owns_file_);

  if (file_path_.empty()) {
    base::FileUtilProxy::CreateTemporary(
        file_task_runner_,
        0,  // No additional file flags.
        base::Bind(&URLFetcherFileWriter::DidCreateTempFile,
                   weak_factory_.GetWeakPtr(),
                   callback));
  } else {
    base::FileUtilProxy::CreateOrOpen(
        file_task_runner_,
        file_path_,
        base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE,
        base::Bind(&URLFetcherFileWriter::DidCreateFile,
                   weak_factory_.GetWeakPtr(),
                   callback,
                   file_path_));
  }
  return ERR_IO_PENDING;
}

int URLFetcherFileWriter::Write(IOBuffer* buffer,
                                int num_bytes,
                                const CompletionCallback& callback) {
  DCHECK_NE(base::kInvalidPlatformFileValue, file_handle_);
  DCHECK(owns_file_);

  ContinueWrite(new DrainableIOBuffer(buffer, num_bytes), callback,
                base::PLATFORM_FILE_OK, 0);
  return ERR_IO_PENDING;
}

int URLFetcherFileWriter::Finish(const CompletionCallback& callback) {
  if (file_handle_ != base::kInvalidPlatformFileValue) {
    base::FileUtilProxy::Close(
        file_task_runner_, file_handle_,
        base::Bind(&URLFetcherFileWriter::DidCloseFile,
                   weak_factory_.GetWeakPtr(),
                   callback));
    file_handle_ = base::kInvalidPlatformFileValue;
    return ERR_IO_PENDING;
  }
  return OK;
}

void URLFetcherFileWriter::ContinueWrite(
    scoped_refptr<DrainableIOBuffer> buffer,
    const CompletionCallback& callback,
    base::PlatformFileError error_code,
    int bytes_written) {
  if (file_handle_ == base::kInvalidPlatformFileValue) {
    // While a write was being done on the file thread, a request
    // to close or disown the file occured on the IO thread.  At
    // this point a request to close the file is pending on the
    // file thread.
    return;
  }

  const int net_error = PlatformFileErrorToNetError(error_code);
  if (net_error != OK) {
    error_code_ = net_error;
    CloseAndDeleteFile();
    callback.Run(net_error);
    return;
  }

  total_bytes_written_ += bytes_written;
  buffer->DidConsume(bytes_written);

  if (buffer->BytesRemaining() > 0) {
    base::FileUtilProxy::Write(
        file_task_runner_, file_handle_,
        total_bytes_written_,  // Append to the end
        buffer->data(), buffer->BytesRemaining(),
        base::Bind(&URLFetcherFileWriter::ContinueWrite,
                   weak_factory_.GetWeakPtr(),
                   buffer,
                   callback));
  } else {
    // Finished writing buffer to the file.
    callback.Run(buffer->size());
  }
}

void URLFetcherFileWriter::DisownFile() {
  // Disowning is done by the delegate's OnURLFetchComplete method.
  // The file should be closed by the time that method is called.
  DCHECK_EQ(base::kInvalidPlatformFileValue, file_handle_);

  owns_file_ = false;
}

void URLFetcherFileWriter::CloseAndDeleteFile() {
  if (!owns_file_)
    return;

  if (file_handle_ == base::kInvalidPlatformFileValue) {
    DeleteFile(base::PLATFORM_FILE_OK);
    return;
  }
  // Close the file if it is open.
  base::FileUtilProxy::Close(
      file_task_runner_, file_handle_,
      base::Bind(&URLFetcherFileWriter::DeleteFile,
                 weak_factory_.GetWeakPtr()));
  file_handle_ = base::kInvalidPlatformFileValue;
}

void URLFetcherFileWriter::DeleteFile(base::PlatformFileError error_code) {
  if (file_path_.empty())
    return;

  base::FileUtilProxy::Delete(
      file_task_runner_, file_path_,
      false,  // No need to recurse, as the path is to a file.
      base::FileUtilProxy::StatusCallback());
  DisownFile();
}

void URLFetcherFileWriter::DidCreateFile(
    const CompletionCallback& callback,
    const base::FilePath& file_path,
    base::PlatformFileError error_code,
    base::PassPlatformFile file_handle,
    bool created) {
  DidCreateFileInternal(callback, file_path, error_code, file_handle);
}

void URLFetcherFileWriter::DidCreateTempFile(
    const CompletionCallback& callback,
    base::PlatformFileError error_code,
    base::PassPlatformFile file_handle,
    const base::FilePath& file_path) {
  DidCreateFileInternal(callback, file_path, error_code, file_handle);
}

void URLFetcherFileWriter::DidCreateFileInternal(
    const CompletionCallback& callback,
    const base::FilePath& file_path,
    base::PlatformFileError error_code,
    base::PassPlatformFile file_handle) {
  const int net_error = PlatformFileErrorToNetError(error_code);
  if (net_error == OK) {
    file_path_ = file_path;
    file_handle_ = file_handle.ReleaseValue();
    total_bytes_written_ = 0;
    owns_file_ = true;
  } else {
    error_code_ = net_error;
  }
  callback.Run(net_error);
}

void URLFetcherFileWriter::DidCloseFile(const CompletionCallback& callback,
                                        base::PlatformFileError error_code) {
  const int net_error = PlatformFileErrorToNetError(error_code);
  if (net_error != OK) {
    error_code_ = net_error;
    CloseAndDeleteFile();
  }
  callback.Run(net_error);
}

}  // namespace net
