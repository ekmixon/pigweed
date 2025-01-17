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

// This file defines the ServerReaderWriter, ServerReader, and ServerWriter
// classes for the raw RPC interface. These classes are used for bidirectional,
// client, and server streaming RPCs.
#pragma once

#include "pw_bytes/span.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/method_info.h"
#include "pw_rpc/internal/method_lookup.h"
#include "pw_rpc/internal/open_call.h"
#include "pw_rpc/internal/server_call.h"
#include "pw_rpc/server.h"

namespace pw::rpc {
namespace internal {

// Forward declarations for internal classes needed in friend statements.
class RawMethod;

namespace test {

template <typename, typename, uint32_t>
class InvocationContext;

}  // namespace test
}  // namespace internal

class RawServerReader;
class RawServerWriter;

// The RawServerReaderWriter is used to send and receive messages in a raw
// bidirectional streaming RPC.
class RawServerReaderWriter : private internal::ServerCall {
 public:
  constexpr RawServerReaderWriter() = default;

  RawServerReaderWriter(RawServerReaderWriter&&) = default;
  RawServerReaderWriter& operator=(RawServerReaderWriter&&) = default;

  // Creates a RawServerReaderWriter that is ready to send responses for a
  // particular RPC. This can be used for testing or to send responses to an RPC
  // that has not been started by a client.
  template <auto kMethod, typename ServiceImpl>
  [[nodiscard]] static RawServerReaderWriter Open(Server& server,
                                                  uint32_t channel_id,
                                                  ServiceImpl& service) {
    return {internal::OpenContext<kMethod, MethodType::kBidirectionalStreaming>(
        server,
        channel_id,
        service,
        internal::MethodLookup::GetRawMethod<
            ServiceImpl,
            internal::MethodInfo<kMethod>::kMethodId>())};
  }

  using internal::Call::active;
  using internal::Call::channel_id;

  // Functions for setting the callbacks.
  using internal::Call::set_on_error;
  using internal::Call::set_on_next;
  using internal::ServerCall::set_on_client_stream_end;

  // Sends a response packet with the given raw payload. The payload can either
  // be in the buffer previously acquired from PayloadBuffer(), or an arbitrary
  // external buffer.
  using internal::Call::Write;

  // Returns a buffer in which a response payload can be built.
  ByteSpan PayloadBuffer() { return AcquirePayloadBuffer(); }

  // Releases a buffer acquired from PayloadBuffer() without sending any data.
  void ReleaseBuffer() { ReleasePayloadBuffer(); }

  Status Finish(Status status = OkStatus()) {
    return CloseAndSendResponse(status);
  }

 protected:
  RawServerReaderWriter(const internal::CallContext& context,
                        MethodType type = MethodType::kBidirectionalStreaming)
      : internal::ServerCall(context, type) {}

  using internal::Call::CloseAndSendResponse;
  using internal::Call::open;  // Deprecated; renamed to active()

 private:
  friend class internal::RawMethod;  // Needed to construct

  template <typename, typename, uint32_t>
  friend class internal::test::InvocationContext;
};

// The RawServerReader is used to receive messages and send a response in a
// raw client streaming RPC.
class RawServerReader : private RawServerReaderWriter {
 public:
  // Creates a RawServerReader that is ready to send a response to a particular
  // RPC. This can be used for testing or to finish an RPC that has not been
  // started by the client.
  template <auto kMethod, typename ServiceImpl>
  [[nodiscard]] static RawServerReader Open(Server& server,
                                            uint32_t channel_id,
                                            ServiceImpl& service) {
    return {internal::OpenContext<kMethod, MethodType::kClientStreaming>(
        server,
        channel_id,
        service,
        internal::MethodLookup::GetRawMethod<
            ServiceImpl,
            internal::MethodInfo<kMethod>::kMethodId>())};
  }

  constexpr RawServerReader() = default;

  RawServerReader(RawServerReader&&) = default;
  RawServerReader& operator=(RawServerReader&&) = default;

  using RawServerReaderWriter::active;
  using RawServerReaderWriter::channel_id;

  using RawServerReaderWriter::set_on_client_stream_end;
  using RawServerReaderWriter::set_on_error;
  using RawServerReaderWriter::set_on_next;

  using RawServerReaderWriter::PayloadBuffer;

  Status Finish(ConstByteSpan response, Status status = OkStatus()) {
    return CloseAndSendResponse(response, status);
  }

 private:
  friend class internal::RawMethod;  // Needed for conversions from ReaderWriter

  template <typename, typename, uint32_t>
  friend class internal::test::InvocationContext;

  RawServerReader(const internal::CallContext& context)
      : RawServerReaderWriter(context, MethodType::kClientStreaming) {}
};

// The RawServerWriter is used to send responses in a raw server streaming RPC.
class RawServerWriter : private RawServerReaderWriter {
 public:
  // Creates a RawServerWriter that is ready to send responses for a particular
  // RPC. This can be used for testing or to send responses to an RPC that has
  // not been started by a client.
  template <auto kMethod, typename ServiceImpl>
  [[nodiscard]] static RawServerWriter Open(Server& server,
                                            uint32_t channel_id,
                                            ServiceImpl& service) {
    return {internal::OpenContext<kMethod, MethodType::kServerStreaming>(
        server,
        channel_id,
        service,
        internal::MethodLookup::GetRawMethod<
            ServiceImpl,
            internal::MethodInfo<kMethod>::kMethodId>())};
  }

  constexpr RawServerWriter() = default;

  RawServerWriter(RawServerWriter&&) = default;
  RawServerWriter& operator=(RawServerWriter&&) = default;

  using RawServerReaderWriter::active;
  using RawServerReaderWriter::channel_id;
  using RawServerReaderWriter::open;

  using RawServerReaderWriter::set_on_error;

  using RawServerReaderWriter::Finish;
  using RawServerReaderWriter::PayloadBuffer;
  using RawServerReaderWriter::ReleaseBuffer;
  using RawServerReaderWriter::Write;

 private:
  template <typename, typename, uint32_t>
  friend class internal::test::InvocationContext;

  friend class internal::RawMethod;

  RawServerWriter(const internal::CallContext& context)
      : RawServerReaderWriter(context, MethodType::kServerStreaming) {}
};

// The RawUnaryResponder is used to send a response in a raw unary RPC.
class RawUnaryResponder : private RawServerReaderWriter {
 public:
  // Creates a RawUnaryResponder that is ready to send responses for a
  // particular RPC. This can be used for testing or to send responses to an RPC
  // that has not been started by a client.
  template <auto kMethod, typename ServiceImpl>
  [[nodiscard]] static RawUnaryResponder Open(Server& server,
                                              uint32_t channel_id,
                                              ServiceImpl& service) {
    return {internal::OpenContext<kMethod, MethodType::kUnary>(
        server,
        channel_id,
        service,
        internal::MethodLookup::GetRawMethod<
            ServiceImpl,
            internal::MethodInfo<kMethod>::kMethodId>())};
  }

  constexpr RawUnaryResponder() = default;

  RawUnaryResponder(RawUnaryResponder&&) = default;
  RawUnaryResponder& operator=(RawUnaryResponder&&) = default;

  using RawServerReaderWriter::active;
  using RawServerReaderWriter::channel_id;

  using RawServerReaderWriter::set_on_error;

  using RawServerReaderWriter::PayloadBuffer;
  using RawServerReaderWriter::ReleaseBuffer;

  Status Finish(ConstByteSpan response, Status status = OkStatus()) {
    return CloseAndSendResponse(response, status);
  }

 private:
  template <typename, typename, uint32_t>
  friend class internal::test::InvocationContext;

  friend class internal::RawMethod;

  RawUnaryResponder(const internal::CallContext& context)
      : RawServerReaderWriter(context, MethodType::kUnary) {}
};

}  // namespace pw::rpc
