// Copyright 2021 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <array>

#include "pw_function/function.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_transfer/internal/client_context.h"
#include "pw_transfer/transfer.raw_rpc.pb.h"
#include "pw_work_queue/work_queue.h"

namespace pw::transfer {

class Client {
 public:
  using CompletionFunc = Function<void(Status)>;

  // Initializes a transfer client on a specified RPC client and channel.
  // Transfers are processed on a work queue so as not to block any RPC threads.
  // The work queue does not have to be unique to the transfer client; it can be
  // shared with other modules (including additional transfer clients).
  //
  // As data is processed within the work queue's context, the original RPC
  // messages received by the transfer service are not available. Therefore,
  // the transfer client requires an additional buffer where transfer data can
  // stored during the context switch.
  //
  // The size of this buffer is the largest amount of bytes that can be sent
  // within a single transfer chunk (read or write), excluding any transport
  // layer overhead. Not all of this size is used to send data -- there is
  // additional overhead in the pw_rpc and pw_transfer protocols (typically
  // ~22B/chunk).
  //
  // An optional max_bytes_to_receive argument can be provided to set the
  // default number of data bytes the client will request from the server at a
  // time. If not provided, this defaults to the size of the data buffer. A
  // larger value can make transfers more efficient as it minimizes the
  // back-and-forth between client and server; however, it also increases the
  // impact of packet loss, potentially requiring larger retransmissions to
  // recover.
  Client(rpc::Client& rpc_client,
         uint32_t channel_id,
         work_queue::WorkQueue& work_queue,
         ByteSpan transfer_data_buffer,
         size_t max_bytes_to_receive = 0)
      : client_(rpc_client, channel_id),
        work_queue_(work_queue),
        max_parameters_(max_bytes_to_receive > 0 ? max_bytes_to_receive
                                                 : transfer_data_buffer.size(),
                        transfer_data_buffer.size()),
        chunk_data_buffer_(transfer_data_buffer) {}

  // Begins a new read transfer for the given transfer ID. The data read from
  // the server is written to the provided writer. Returns OK if the transfer is
  // successfully started. When the transfer finishes (successfully or not), the
  // completion callback is invoked with the overall status.
  Status Read(uint32_t transfer_id,
              stream::Writer& output,
              CompletionFunc&& on_completion);

  // Begins a new write transfer for the given transfer ID. Data from the
  // provided writer is sent to the server. When the transfer finishes
  // (successfully or not), the completion callback is invoked with the overall
  // status.
  Status Write(uint32_t transfer_id,
               stream::Reader& input,
               CompletionFunc&& on_completion);

 private:
  using Transfer = pw_rpc::raw::Transfer;
  using ClientContext = internal::ClientContext;

  enum Type : bool { kRead, kWrite };

  Status StartNewTransfer(uint32_t transfer_id,
                          stream::Stream& stream,
                          CompletionFunc&& on_completion,
                          Type type);
  ClientContext* GetActiveTransfer(uint32_t transfer_id);

  // Function called when a chunk is received, from the context of the RPC
  // client thread.
  void OnChunk(ConstByteSpan data, Type type);

  Transfer::Client client_;
  work_queue::WorkQueue& work_queue_;

  rpc::RawClientReaderWriter read_stream_;
  rpc::RawClientReaderWriter write_stream_;

  // TODO(frolv): Make this size configurable.
  std::array<ClientContext, 1> transfer_contexts_
      PW_GUARDED_BY(transfer_context_mutex_);
  sync::Mutex transfer_context_mutex_;

  internal::TransferParameters max_parameters_;
  internal::ChunkDataBuffer chunk_data_buffer_;
};

}  // namespace pw::transfer
