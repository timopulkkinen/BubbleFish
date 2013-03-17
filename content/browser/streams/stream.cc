// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "content/browser/streams/stream_read_observer.h"
#include "content/browser/streams/stream_registry.h"
#include "content/browser/streams/stream_write_observer.h"
#include "net/base/io_buffer.h"

namespace {
// Start throttling the connection at about 1MB.
const size_t kDeferSizeThreshold = 40 * 32768;
}

namespace content {

Stream::Stream(StreamRegistry* registry,
               StreamWriteObserver* write_observer,
               const GURL& security_origin,
               const GURL& url)
    : bytes_read_(0),
      can_add_data_(true),
      security_origin_(security_origin),
      url_(url),
      data_length_(0),
      registry_(registry),
      read_observer_(NULL),
      write_observer_(write_observer),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  CreateByteStream(base::MessageLoopProxy::current(),
                   base::MessageLoopProxy::current(),
                   kDeferSizeThreshold,
                   &writer_,
                   &reader_);

  // Setup callback for writing.
  writer_->RegisterCallback(base::Bind(&Stream::OnSpaceAvailable,
                                       weak_ptr_factory_.GetWeakPtr()));
  reader_->RegisterCallback(base::Bind(&Stream::OnDataAvailable,
                                       weak_ptr_factory_.GetWeakPtr()));

  registry_->RegisterStream(this);
}

Stream::~Stream() {
}

bool Stream::SetReadObserver(StreamReadObserver* observer) {
  if (read_observer_)
    return false;
  read_observer_ = observer;
  return true;
}

void Stream::RemoveReadObserver(StreamReadObserver* observer) {
  DCHECK(observer == read_observer_);
  read_observer_ = NULL;
}

void Stream::AddData(scoped_refptr<net::IOBuffer> buffer, size_t size) {
  can_add_data_ = writer_->Write(buffer, size);
}

void Stream::Finalize() {
  writer_->Close(DOWNLOAD_INTERRUPT_REASON_NONE);
  writer_.reset(NULL);

  OnDataAvailable();
}

Stream::StreamState Stream::ReadRawData(net::IOBuffer* buf,
                                        int buf_size,
                                        int* bytes_read) {
  if (!data_) {
    data_length_ = 0;
    bytes_read_ = 0;
    ByteStreamReader::StreamState state = reader_->Read(&data_, &data_length_);
    switch (state) {
      case ByteStreamReader::STREAM_HAS_DATA:
        break;
      case ByteStreamReader::STREAM_COMPLETE:
        registry_->UnregisterStream(url());
        return STREAM_COMPLETE;
      case ByteStreamReader::STREAM_EMPTY:
        return STREAM_EMPTY;
    }
  }

  size_t remaining_bytes = data_length_ - bytes_read_;
  size_t to_read =
      static_cast<size_t>(buf_size) < remaining_bytes ?
      buf_size : remaining_bytes;
  memcpy(buf->data(), data_->data() + bytes_read_, to_read);
  bytes_read_ += to_read;
  if (bytes_read_ >= data_length_)
    data_ = NULL;

  *bytes_read = to_read;
  return STREAM_HAS_DATA;
}

void Stream::OnSpaceAvailable() {
  can_add_data_ = true;
  write_observer_->OnSpaceAvailable(this);
}

void Stream::OnDataAvailable() {
  if (read_observer_)
    read_observer_->OnDataAvailable(this);
}

}  // namespace content

