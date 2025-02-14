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

#include "pw_transfer/client.h"

#include <cstring>

#include "gtest/gtest.h"
#include "pw_assert/check.h"
#include "pw_bytes/array.h"
#include "pw_rpc/raw/client_testing.h"
#include "pw_transfer_private/chunk_testing.h"

namespace pw::transfer::test {
namespace {

using internal::Chunk;
using pw_rpc::raw::Transfer;

class ReadTransfer : public ::testing::Test {
 protected:
  ReadTransfer(size_t max_bytes_to_receive = 0)
      : client_(context_.client(),
                context_.channel().id(),
                work_queue_,
                data_buffer_,
                max_bytes_to_receive),
        work_queue_({}) {}

  rpc::RawClientTestContext<> context_;

  Client client_;
  std::array<std::byte, 64> data_buffer_;

  // The transfer client does not currently use the work queue.
  work_queue::WorkQueue work_queue_;
};

constexpr auto kData32 = bytes::Initialized<32>([](size_t i) { return i; });
constexpr auto kData64 = bytes::Initialized<64>([](size_t i) { return i; });

TEST_F(ReadTransfer, SingleChunk) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  client_.Read(3, writer, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 3u);
  EXPECT_EQ(c0.offset, 0u);
  EXPECT_EQ(c0.pending_bytes.value(), 64u);

  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(
      {.transfer_id = 3u, .offset = 0, .data = kData32, .remaining_bytes = 0}));
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 3u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
  EXPECT_EQ(std::memcmp(writer.data(), kData32.data(), writer.bytes_written()),
            0);
}

TEST_F(ReadTransfer, MultiChunk) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  client_.Read(4, writer, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 4u);
  EXPECT_EQ(c0.offset, 0u);
  EXPECT_EQ(c0.pending_bytes.value(), 64u);

  constexpr ConstByteSpan data(kData32);
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 4u, .offset = 0, .data = data.first(16)}));
  ASSERT_EQ(payloads.size(), 1u);

  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 4u,
                   .offset = 16,
                   .data = data.subspan(16),
                   .remaining_bytes = 0}));
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 4u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
  EXPECT_EQ(std::memcmp(writer.data(), kData32.data(), writer.bytes_written()),
            0);
}

class ReadTransferMaxBytes32 : public ReadTransfer {
 protected:
  ReadTransferMaxBytes32() : ReadTransfer(/*max_bytes_to_receive=*/32) {}
};

TEST_F(ReadTransferMaxBytes32, SetsPendingBytesFromConstructorArg) {
  stream::MemoryWriterBuffer<64> writer;
  client_.Read(5, writer, [](Status) {});

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 5u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 32u);
}

TEST_F(ReadTransferMaxBytes32, SetsPendingBytesFromWriterLimit) {
  stream::MemoryWriterBuffer<16> small_writer;
  client_.Read(5, small_writer, [](Status) {});

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 5u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 16u);
}

TEST_F(ReadTransferMaxBytes32, MultiParameters) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  client_.Read(6, writer, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 6u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 32u);

  constexpr ConstByteSpan data(kData64);
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 6u, .offset = 0, .data = data.first(32)}));
  ASSERT_EQ(payloads.size(), 2u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  // Second parameters chunk.
  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 6u);
  EXPECT_EQ(c1.offset, 32u);
  ASSERT_EQ(c1.pending_bytes.value(), 32u);

  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 6u,
                   .offset = 32,
                   .data = data.subspan(32),
                   .remaining_bytes = 0}));
  ASSERT_EQ(payloads.size(), 3u);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 6u);
  ASSERT_TRUE(c2.status.has_value());
  EXPECT_EQ(c2.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
  EXPECT_EQ(std::memcmp(writer.data(), data.data(), writer.bytes_written()), 0);
}

TEST_F(ReadTransfer, UnexpectedOffset) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  client_.Read(7, writer, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 7u);
  EXPECT_EQ(c0.offset, 0u);
  EXPECT_EQ(c0.pending_bytes.value(), 64u);

  constexpr ConstByteSpan data(kData32);
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 7u, .offset = 0, .data = data.first(16)}));
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  // Send a chunk with an incorrect offset. The client should resend parameters.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 7u,
                   .offset = 8,  // wrong!
                   .data = data.subspan(16),
                   .remaining_bytes = 0}));
  ASSERT_EQ(payloads.size(), 2u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 7u);
  EXPECT_EQ(c1.offset, 16u);
  EXPECT_EQ(c1.pending_bytes.value(), 48u);

  // Send the correct chunk, completing the transfer.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 7u,
                   .offset = 16,
                   .data = data.subspan(16),
                   .remaining_bytes = 0}));
  ASSERT_EQ(payloads.size(), 3u);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 7u);
  ASSERT_TRUE(c2.status.has_value());
  EXPECT_EQ(c2.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
  EXPECT_EQ(std::memcmp(writer.data(), kData32.data(), writer.bytes_written()),
            0);
}

TEST_F(ReadTransferMaxBytes32, TooMuchData) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  client_.Read(8, writer, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 8u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 32u);

  constexpr ConstByteSpan data(kData64);

  // pending_bytes == 32
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 8u, .offset = 0, .data = data.first(16)}));

  // pending_bytes == 16
  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(
      {.transfer_id = 8u, .offset = 16, .data = data.subspan(16, 8)}));

  // pending_bytes == 8, send 16 instead.
  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(
      {.transfer_id = 8u, .offset = 24, .data = data.subspan(24, 16)}));

  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 8u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), Status::Internal());

  EXPECT_EQ(transfer_status, Status::Internal());
}

TEST_F(ReadTransfer, ServerError) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  client_.Read(9, writer, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 9u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 64u);

  // Server sends an error. Client should not respond and terminate the
  // transfer.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 9u, .status = Status::NotFound()}));
  ASSERT_EQ(payloads.size(), 1u);

  EXPECT_EQ(transfer_status, Status::NotFound());
}

TEST_F(ReadTransfer, OnlySendsParametersOnceAfterDrop) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  client_.Read(10, writer, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 10u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 64u);

  constexpr ConstByteSpan data(kData64);

  // Send the first 8 bytes of the transfer.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 10u, .offset = 0, .data = data.first(8)}));

  // Skip offset 8, send the rest starting from 16.
  for (uint32_t offset = 16; offset < data.size(); offset += 8) {
    context_.server().SendServerStream<Transfer::Read>(
        EncodeChunk({.transfer_id = 10u,
                     .offset = offset,
                     .data = data.subspan(offset, 8)}));
  }

  // Only one parameters update should be sent, with the offset of the initial
  // dropped packet.
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 10u);
  EXPECT_EQ(c1.offset, 8u);
  ASSERT_EQ(c1.pending_bytes.value(), 56u);

  // Send the remaining data to complete the transfer.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 10u,
                   .offset = 8,
                   .data = data.subspan(8, 56),
                   .remaining_bytes = 0}));
  ASSERT_EQ(payloads.size(), 3u);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 10u);
  ASSERT_TRUE(c2.status.has_value());
  EXPECT_EQ(c2.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
}

TEST_F(ReadTransfer, ResendsParametersIfSentRepeatedChunkDuringRecovery) {
  stream::MemoryWriterBuffer<64> writer;
  Status transfer_status = Status::Unknown();

  client_.Read(11, writer, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // First transfer parameters chunk is sent.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Read>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 11u);
  EXPECT_EQ(c0.offset, 0u);
  ASSERT_EQ(c0.pending_bytes.value(), 64u);

  constexpr ConstByteSpan data(kData64);

  // Send the first 8 bytes of the transfer.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 11u, .offset = 0, .data = data.first(8)}));

  // Skip offset 8, send the rest starting from 16.
  for (uint32_t offset = 16; offset < data.size(); offset += 8) {
    context_.server().SendServerStream<Transfer::Read>(
        EncodeChunk({.transfer_id = 11u,
                     .offset = offset,
                     .data = data.subspan(offset, 8)}));
  }

  // Only one parameters update should be sent, with the offset of the initial
  // dropped packet.
  ASSERT_EQ(payloads.size(), 2u);

  const Chunk last_chunk = {
      .transfer_id = 11u, .offset = 56, .data = data.subspan(56)};

  // Re-send the final chunk of the block.
  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(last_chunk));

  // The original drop parameters should be re-sent.
  ASSERT_EQ(payloads.size(), 3u);
  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 11u);
  EXPECT_EQ(c2.offset, 8u);
  ASSERT_EQ(c2.pending_bytes.value(), 56u);

  // Do it again.
  context_.server().SendServerStream<Transfer::Read>(EncodeChunk(last_chunk));
  ASSERT_EQ(payloads.size(), 4u);
  Chunk c3 = DecodeChunk(payloads[3]);
  EXPECT_EQ(c3.transfer_id, 11u);
  EXPECT_EQ(c3.offset, 8u);
  ASSERT_EQ(c3.pending_bytes.value(), 56u);

  // Finish the transfer normally.
  context_.server().SendServerStream<Transfer::Read>(
      EncodeChunk({.transfer_id = 11u,
                   .offset = 8,
                   .data = data.subspan(8, 56),
                   .remaining_bytes = 0}));
  ASSERT_EQ(payloads.size(), 5u);

  Chunk c4 = DecodeChunk(payloads[4]);
  EXPECT_EQ(c4.transfer_id, 11u);
  ASSERT_TRUE(c4.status.has_value());
  EXPECT_EQ(c4.status.value(), OkStatus());

  EXPECT_EQ(transfer_status, OkStatus());
}

class WriteTransfer : public ::testing::Test {
 protected:
  WriteTransfer()
      : client_(context_.client(),
                context_.channel().id(),
                work_queue_,
                data_buffer_),
        work_queue_({}) {}

  rpc::RawClientTestContext<> context_;

  Client client_;
  std::array<std::byte, 64> data_buffer_;

  // The transfer client does not currently use the work queue.
  work_queue::WorkQueue work_queue_;
};

TEST_F(WriteTransfer, SingleChunk) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  client_.Write(3, reader, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 3u);

  // Send transfer parameters.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 3,
                   .pending_bytes = 64,
                   .max_chunk_size_bytes = 32,
                   .offset = 0}));

  // Client should send a data chunk and the final chunk.
  ASSERT_EQ(payloads.size(), 3u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 3u);
  EXPECT_EQ(c1.offset, 0u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData32.data(), c1.data.size()), 0);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 3u);
  ASSERT_TRUE(c2.remaining_bytes.has_value());
  EXPECT_EQ(c2.remaining_bytes.value(), 0u);

  EXPECT_EQ(transfer_status, Status::Unknown());

  // Send the final status chunk to complete the transfer.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 3, .status = OkStatus()}));
  EXPECT_EQ(payloads.size(), 3u);
  EXPECT_EQ(transfer_status, OkStatus());
}

TEST_F(WriteTransfer, MultiChunk) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  client_.Write(4, reader, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 4u);

  // Send transfer parameters with a chunk size smaller than the data.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 4,
                   .pending_bytes = 64,
                   .max_chunk_size_bytes = 16,
                   .offset = 0}));

  // Client should send two data chunks and the final chunk.
  ASSERT_EQ(payloads.size(), 4u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 4u);
  EXPECT_EQ(c1.offset, 0u);
  EXPECT_EQ(std::memcmp(c1.data.data(), kData32.data(), c1.data.size()), 0);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 4u);
  EXPECT_EQ(c2.offset, 16u);
  EXPECT_EQ(
      std::memcmp(c2.data.data(), kData32.data() + c2.offset, c2.data.size()),
      0);

  Chunk c3 = DecodeChunk(payloads[3]);
  EXPECT_EQ(c3.transfer_id, 4u);
  ASSERT_TRUE(c3.remaining_bytes.has_value());
  EXPECT_EQ(c3.remaining_bytes.value(), 0u);

  EXPECT_EQ(transfer_status, Status::Unknown());

  // Send the final status chunk to complete the transfer.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 4, .status = OkStatus()}));
  EXPECT_EQ(payloads.size(), 4u);
  EXPECT_EQ(transfer_status, OkStatus());
}

TEST_F(WriteTransfer, OutOfOrder_SeekSupported) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  client_.Write(5, reader, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 5u);

  // Send transfer parameters with a nonzero offset, requesting a seek.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 5,
                   .pending_bytes = 64,
                   .max_chunk_size_bytes = 32,
                   .offset = 16}));

  // Client should send a data chunk and the final chunk.
  ASSERT_EQ(payloads.size(), 3u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 5u);
  EXPECT_EQ(c1.offset, 16u);
  EXPECT_EQ(
      std::memcmp(c1.data.data(), kData32.data() + c1.offset, c1.data.size()),
      0);

  Chunk c2 = DecodeChunk(payloads[2]);
  EXPECT_EQ(c2.transfer_id, 5u);
  ASSERT_TRUE(c2.remaining_bytes.has_value());
  EXPECT_EQ(c2.remaining_bytes.value(), 0u);

  EXPECT_EQ(transfer_status, Status::Unknown());

  // Send the final status chunk to complete the transfer.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 5, .status = OkStatus()}));
  EXPECT_EQ(payloads.size(), 3u);
  EXPECT_EQ(transfer_status, OkStatus());
}

class FakeNonSeekableReader : public stream::NonSeekableReader {
 private:
  StatusWithSize DoRead(ByteSpan) final {
    return StatusWithSize::Unimplemented();
  }
};

TEST_F(WriteTransfer, OutOfOrder_SeekNotSupported) {
  FakeNonSeekableReader reader;
  Status transfer_status = Status::Unknown();

  client_.Write(6, reader, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 6u);

  // Send transfer parameters with a nonzero offset, requesting a seek.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 6,
                   .pending_bytes = 64,
                   .max_chunk_size_bytes = 32,
                   .offset = 16}));

  // Client should send a status chunk and end the transfer.
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 6u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), Status::Unimplemented());

  EXPECT_EQ(transfer_status, Status::Unimplemented());
}

TEST_F(WriteTransfer, ServerError) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  client_.Write(7, reader, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 7u);

  // Send an error from the server.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 7, .status = Status::NotFound()}));

  // Client should not respond and terminate the transfer.
  EXPECT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::NotFound());
}

TEST_F(WriteTransfer, MalformedParametersChunk) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  client_.Write(8, reader, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 8u);

  // Send an invalid transfer parameters chunk without pending_bytes.
  context_.server().SendServerStream<Transfer::Write>(
      EncodeChunk({.transfer_id = 8, .max_chunk_size_bytes = 32}));

  // Client should send a status chunk and end the transfer.
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 8u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), Status::InvalidArgument());

  EXPECT_EQ(transfer_status, Status::InvalidArgument());
}

TEST_F(WriteTransfer, AbortIfZeroBytesAreRequested) {
  stream::MemoryReader reader(kData32);
  Status transfer_status = Status::Unknown();

  client_.Write(9, reader, [&transfer_status](Status status) {
    transfer_status = status;
  });

  // The client begins by just sending the transfer ID.
  rpc::PayloadsView payloads =
      context_.output().payloads<Transfer::Write>(context_.channel().id());
  ASSERT_EQ(payloads.size(), 1u);
  EXPECT_EQ(transfer_status, Status::Unknown());

  Chunk c0 = DecodeChunk(payloads[0]);
  EXPECT_EQ(c0.transfer_id, 9u);

  // Send an invalid transfer parameters chunk with 0 pending_bytes.
  context_.server().SendServerStream<Transfer::Write>(EncodeChunk(
      {.transfer_id = 9, .pending_bytes = 0, .max_chunk_size_bytes = 32}));

  // Client should send a status chunk and end the transfer.
  ASSERT_EQ(payloads.size(), 2u);

  Chunk c1 = DecodeChunk(payloads[1]);
  EXPECT_EQ(c1.transfer_id, 9u);
  ASSERT_TRUE(c1.status.has_value());
  EXPECT_EQ(c1.status.value(), Status::Internal());

  EXPECT_EQ(transfer_status, Status::Internal());
}

}  // namespace
}  // namespace pw::transfer::test
