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

#include "pw_log_rpc/log_service.h"

#include <array>
#include <cstdint>
#include <limits>

#include "gtest/gtest.h"
#include "pw_assert/check.h"
#include "pw_containers/vector.h"
#include "pw_log/log.h"
#include "pw_log/proto/log.pwpb.h"
#include "pw_log/proto_utils.h"
#include "pw_log_tokenized/metadata.h"
#include "pw_protobuf/decoder.h"
#include "pw_result/result.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/raw/fake_channel_output.h"
#include "pw_rpc/raw/test_method_context.h"
#include "pw_sync/mutex.h"

namespace pw::log_rpc {
namespace {

using log::pw_rpc::raw::Logs;

#define LOG_SERVICE_METHOD_CONTEXT \
  PW_RAW_TEST_METHOD_CONTEXT(LogService, Listen, 6, 128)

constexpr size_t kMaxMessageSize = 50;
constexpr size_t kMaxLogEntrySize =
    RpcLogDrain::kMinEntrySizeWithoutPayload + kMaxMessageSize;
static_assert(RpcLogDrain::kMinEntryBufferSize < kMaxLogEntrySize);
constexpr size_t kMultiSinkBufferSize = kMaxLogEntrySize * 10;
constexpr size_t kMaxDrains = 3;
constexpr char kMessage[] = "message";
// A message small enough to fit encoded in LogServiceTest::entry_encode_buffer_
// but large enough to not fit in LogServiceTest::small_buffer_.
constexpr char kLongMessage[] =
    "This is a long log message that will be dropped.";
static_assert(sizeof(kLongMessage) < kMaxMessageSize);
static_assert(sizeof(kLongMessage) > RpcLogDrain::kMinEntryBufferSize);
std::array<std::byte, 1> rpc_request_buffer;
constexpr auto kSampleMetadata =
    log_tokenized::Metadata::Set<PW_LOG_LEVEL_INFO, 123, 0x03, __LINE__>();
constexpr auto kDropMessageMetadata =
    log_tokenized::Metadata::Set<0, 0, 0, 0>();
constexpr int64_t kSampleTimestamp = 1000;

// `LogServiceTest` sets up a logging environment for testing with a `MultiSink`
// for log entries, and multiple `RpcLogDrain`s for consuming such log entries.
// It includes methods to add log entries to the `MultiSink`, and buffers for
// encoding and retrieving log entries. Tests can choose how many entries to
// add to the multisink, and which drain to use.
class LogServiceTest : public ::testing::Test {
 public:
  LogServiceTest() : multisink_(multisink_buffer_), drain_map_(drains_) {
    for (auto& drain : drain_map_.drains()) {
      multisink_.AttachDrain(drain);
    }
  }

  void AddLogEntries(size_t log_count,
                     std::string_view message,
                     log_tokenized::Metadata metadata,
                     int64_t timestamp) {
    for (size_t i = 0; i < log_count; ++i) {
      ASSERT_TRUE(AddLogEntry(message, metadata, timestamp).ok());
    }
  }

  StatusWithSize AddLogEntry(std::string_view message,
                             log_tokenized::Metadata metadata,
                             int64_t timestamp) {
    Result<ConstByteSpan> encoded_log_result =
        log::EncodeTokenizedLog(metadata,
                                std::as_bytes(std::span(message)),
                                timestamp,
                                entry_encode_buffer_);
    PW_TRY_WITH_SIZE(encoded_log_result.status());
    multisink_.HandleEntry(encoded_log_result.value());
    return StatusWithSize(encoded_log_result.value().size());
  }

 protected:
  std::array<std::byte, kMultiSinkBufferSize> multisink_buffer_;
  multisink::MultiSink multisink_;
  RpcLogDrainMap drain_map_;
  std::array<std::byte, kMaxLogEntrySize> entry_encode_buffer_;

  // Drain Buffers
  std::array<std::byte, kMaxLogEntrySize> drain_buffer1_;
  std::array<std::byte, kMaxLogEntrySize> drain_buffer2_;
  std::array<std::byte, RpcLogDrain::kMinEntryBufferSize> small_buffer_;
  static constexpr uint32_t kIgnoreWriterErrorsDrainId = 1;
  static constexpr uint32_t kCloseWriterOnErrorDrainId = 2;
  static constexpr uint32_t kSmallBufferDrainId = 3;
  sync::Mutex shared_mutex_;
  std::array<RpcLogDrain, kMaxDrains> drains_{
      RpcLogDrain(kIgnoreWriterErrorsDrainId,
                  drain_buffer1_,
                  shared_mutex_,
                  RpcLogDrain::LogDrainErrorHandling::kIgnoreWriterErrors),
      RpcLogDrain(
          kCloseWriterOnErrorDrainId,
          drain_buffer2_,
          shared_mutex_,
          RpcLogDrain::LogDrainErrorHandling::kCloseStreamOnWriterError),
      RpcLogDrain(kSmallBufferDrainId,
                  small_buffer_,
                  shared_mutex_,
                  RpcLogDrain::LogDrainErrorHandling::kIgnoreWriterErrors),
  };
};
struct TestLogEntry {
  log_tokenized::Metadata metadata = kSampleMetadata;
  int64_t timestamp = 0;
  uint32_t dropped = 0;
  ConstByteSpan tokenized_data = {};
};

// Unpacks a `LogEntry` proto buffer and compares it with the expected data.
void VerifyLogEntry(protobuf::Decoder& entry_decoder,
                    const TestLogEntry& expected_entry) {
  ConstByteSpan tokenized_data;
  if (!expected_entry.tokenized_data.empty()) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(), 1u);  // message [tokenized]
    ASSERT_TRUE(entry_decoder.ReadBytes(&tokenized_data).ok());
    if (tokenized_data.size() != expected_entry.tokenized_data.size()) {
      PW_LOG_ERROR(
          "actual: '%s', expected: '%s'",
          reinterpret_cast<const char*>(tokenized_data.begin()),
          reinterpret_cast<const char*>(expected_entry.tokenized_data.begin()));
    }
    EXPECT_EQ(tokenized_data.size(), expected_entry.tokenized_data.size());
    EXPECT_EQ(std::memcmp(tokenized_data.begin(),
                          expected_entry.tokenized_data.begin(),
                          expected_entry.tokenized_data.size()),
              0);
  }
  if (expected_entry.metadata.level()) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(), 2u);  // line_level
    uint32_t line_level;
    ASSERT_TRUE(entry_decoder.ReadUint32(&line_level).ok());
    EXPECT_EQ(expected_entry.metadata.level(),
              line_level & PW_LOG_LEVEL_BITMASK);
    EXPECT_EQ(expected_entry.metadata.line_number(),
              (line_level & ~PW_LOG_LEVEL_BITMASK) >> PW_LOG_LEVEL_BITS);
  }
  if (expected_entry.metadata.flags()) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(), 3u);  // flags
    uint32_t flags;
    ASSERT_TRUE(entry_decoder.ReadUint32(&flags).ok());
    EXPECT_EQ(expected_entry.metadata.flags(), flags);
  }
  if (expected_entry.timestamp) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_TRUE(entry_decoder.FieldNumber() == 4u       // timestamp
                || entry_decoder.FieldNumber() == 5u);  // time_since_last_entry
    int64_t timestamp;
    ASSERT_TRUE(entry_decoder.ReadInt64(&timestamp).ok());
    EXPECT_EQ(expected_entry.timestamp, timestamp);
  }
  if (expected_entry.dropped) {
    ASSERT_EQ(entry_decoder.Next(), OkStatus());
    ASSERT_EQ(entry_decoder.FieldNumber(), 6u);  // dropped
    uint32_t dropped;
    ASSERT_TRUE(entry_decoder.ReadUint32(&dropped).ok());
    EXPECT_EQ(expected_entry.dropped, dropped);
  }
}

// Verifies a stream of log entries, returning the total count found.
size_t VerifyLogEntries(protobuf::Decoder& entries_decoder,
                        Vector<TestLogEntry>& expected_entries_stack) {
  size_t entries_found = 0;
  while (entries_decoder.Next().ok()) {
    ConstByteSpan entry;
    EXPECT_TRUE(entries_decoder.ReadBytes(&entry).ok());
    protobuf::Decoder entry_decoder(entry);
    if (expected_entries_stack.empty()) {
      break;
    }
    VerifyLogEntry(entry_decoder, expected_entries_stack.back());
    expected_entries_stack.pop_back();
    ++entries_found;
  }
  return entries_found;
}

size_t CountLogEntries(protobuf::Decoder& entries_decoder) {
  size_t entries_found = 0;
  while (entries_decoder.Next().ok()) {
    ++entries_found;
  }
  return entries_found;
}

TEST_F(LogServiceTest, AssignWriter) {
  // Drains don't have writers.
  for (auto& drain : drain_map_.drains()) {
    EXPECT_EQ(drain.Flush(), Status::Unavailable());
  }

  // Create context directed to drain with ID 1.
  RpcLogDrain& active_drain = drains_[0];
  const uint32_t drain_channel_id = active_drain.channel_id();
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_);
  context.set_channel_id(drain_channel_id);

  // Call RPC, which sets the drain's writer.
  context.call(rpc_request_buffer);
  EXPECT_EQ(active_drain.Flush(), OkStatus());

  // Other drains are still missing writers.
  for (auto& drain : drain_map_.drains()) {
    if (drain.channel_id() != drain_channel_id) {
      EXPECT_EQ(drain.Flush(), Status::Unavailable());
    }
  }

  // Calling an ongoing log stream must not change the active drain's
  // writer, and the second writer must not get any responses.
  LOG_SERVICE_METHOD_CONTEXT second_call_context(drain_map_);
  second_call_context.set_channel_id(drain_channel_id);
  second_call_context.call(rpc_request_buffer);
  EXPECT_EQ(active_drain.Flush(), OkStatus());
  ASSERT_TRUE(second_call_context.done());
  EXPECT_EQ(second_call_context.responses().size(), 0u);

  // Setting a new writer on a closed stream is allowed.
  ASSERT_EQ(active_drain.Close(), OkStatus());
  LOG_SERVICE_METHOD_CONTEXT third_call_context(drain_map_);
  third_call_context.set_channel_id(drain_channel_id);
  third_call_context.call(rpc_request_buffer);
  EXPECT_EQ(active_drain.Flush(), OkStatus());
  ASSERT_FALSE(third_call_context.done());
  EXPECT_EQ(third_call_context.responses().size(), 1u);
  EXPECT_EQ(active_drain.Close(), OkStatus());
}

TEST_F(LogServiceTest, StartAndEndStream) {
  RpcLogDrain& active_drain = drains_[2];
  const uint32_t drain_channel_id = active_drain.channel_id();
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_);
  context.set_channel_id(drain_channel_id);

  // Add log entries.
  const size_t total_entries = 10;
  AddLogEntries(total_entries, kMessage, kSampleMetadata, kSampleTimestamp);
  // Request logs.
  context.call(rpc_request_buffer);
  EXPECT_EQ(active_drain.Flush(), OkStatus());

  // Not done until the stream is finished.
  ASSERT_FALSE(context.done());
  active_drain.Close();
  ASSERT_TRUE(context.done());

  EXPECT_EQ(context.status(), OkStatus());
  // There is at least 1 response with multiple log entries packed.
  EXPECT_GE(context.responses().size(), 1u);

  // Verify data in responses.
  Vector<TestLogEntry, total_entries> message_stack;
  for (size_t i = 0; i < total_entries; ++i) {
    message_stack.push_back({.timestamp = kSampleTimestamp,
                             .tokenized_data = std::as_bytes(
                                 std::span(std::string_view(kMessage)))});
  }
  size_t entries_found = 0;
  for (auto& response : context.responses()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += VerifyLogEntries(entry_decoder, message_stack);
  }
  EXPECT_EQ(entries_found, total_entries);
}

TEST_F(LogServiceTest, HandleDropped) {
  RpcLogDrain& active_drain = drains_[0];
  const uint32_t drain_channel_id = active_drain.channel_id();
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_);
  context.set_channel_id(drain_channel_id);

  // Add log entries.
  const size_t total_entries = 5;
  const uint32_t total_drop_count = 2;
  AddLogEntries(total_entries, kMessage, kSampleMetadata, kSampleTimestamp);
  multisink_.HandleDropped(total_drop_count);

  // Request logs.
  context.call(rpc_request_buffer);
  EXPECT_EQ(active_drain.Flush(), OkStatus());
  active_drain.Close();
  ASSERT_EQ(context.status(), OkStatus());
  // There is at least 1 response with multiple log entries packed.
  ASSERT_GE(context.responses().size(), 1u);

  // Add create expected messages in a stack to match the order they arrive in.
  Vector<TestLogEntry, total_entries + 1> message_stack;
  message_stack.push_back(
      {.metadata = kDropMessageMetadata, .dropped = total_drop_count});
  for (size_t i = 0; i < total_entries; ++i) {
    message_stack.push_back({.timestamp = kSampleTimestamp,
                             .tokenized_data = std::as_bytes(
                                 std::span(std::string_view(kMessage)))});
  }

  // Verify data in responses.
  size_t entries_found = 0;
  for (auto& response : context.responses()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += VerifyLogEntries(entry_decoder, message_stack);
  }
  // Expect an extra message with the drop count.
  EXPECT_EQ(entries_found, total_entries + 1);
}

TEST_F(LogServiceTest, HandleSmallBuffer) {
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_);
  context.set_channel_id(kSmallBufferDrainId);
  auto small_buffer_drain =
      drain_map_.GetDrainFromChannelId(kSmallBufferDrainId);
  ASSERT_TRUE(small_buffer_drain.ok());

  // Add log entries.
  const size_t total_entries = 5;
  const uint32_t total_drop_count = total_entries;
  AddLogEntries(total_entries, kLongMessage, kSampleMetadata, kSampleTimestamp);
  // Request logs.
  context.call(rpc_request_buffer);
  EXPECT_EQ(small_buffer_drain.value()->Flush(), OkStatus());
  EXPECT_EQ(small_buffer_drain.value()->Close(), OkStatus());
  ASSERT_EQ(context.status(), OkStatus());
  ASSERT_GE(context.responses().size(), 1u);

  Vector<TestLogEntry, total_entries + 1> message_stack;
  message_stack.push_back(
      {.metadata = kDropMessageMetadata, .dropped = total_drop_count});

  // Verify data in responses.
  size_t entries_found = 0;
  for (auto& response : context.responses()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += VerifyLogEntries(entry_decoder, message_stack);
  }
  // No messages fit the buffer, expect a drop message.
  EXPECT_EQ(entries_found, 1u);
}

TEST_F(LogServiceTest, FlushDrainWithoutMultisink) {
  auto& detached_drain = drains_[0];
  multisink_.DetachDrain(detached_drain);
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_);
  context.set_channel_id(detached_drain.channel_id());

  // Add log entries.
  const size_t total_entries = 5;
  AddLogEntries(total_entries, kMessage, kSampleMetadata, kSampleTimestamp);
  // Request logs.
  context.call(rpc_request_buffer);
  EXPECT_EQ(detached_drain.Close(), OkStatus());
  ASSERT_EQ(context.status(), OkStatus());
  EXPECT_EQ(context.responses().size(), 0u);
}

TEST_F(LogServiceTest, LargeLogEntry) {
  const TestLogEntry expected_entry{
      .metadata =
          log_tokenized::Metadata::Set<PW_LOG_LEVEL_WARN,
                                       (1 << PW_LOG_TOKENIZED_MODULE_BITS) - 1,
                                       (1 << PW_LOG_TOKENIZED_FLAG_BITS) - 1,
                                       (1 << PW_LOG_TOKENIZED_LINE_BITS) - 1>(),
      .timestamp = std::numeric_limits<int64_t>::max(),
      .tokenized_data = std::as_bytes(std::span(kMessage)),
  };

  // Add entry to multisink.
  log::LogEntry::MemoryEncoder encoder(entry_encode_buffer_);
  encoder.WriteMessage(expected_entry.tokenized_data);
  encoder.WriteLineLevel(
      (expected_entry.metadata.level() & PW_LOG_LEVEL_BITMASK) |
      ((expected_entry.metadata.line_number() << PW_LOG_LEVEL_BITS) &
       ~PW_LOG_LEVEL_BITMASK));
  encoder.WriteFlags(expected_entry.metadata.flags());
  encoder.WriteTimestamp(expected_entry.timestamp);
  ASSERT_EQ(encoder.status(), OkStatus());
  multisink_.HandleEntry(encoder);

  // Start log stream.
  RpcLogDrain& active_drain = drains_[0];
  const uint32_t drain_channel_id = active_drain.channel_id();
  LOG_SERVICE_METHOD_CONTEXT context(drain_map_);
  context.set_channel_id(drain_channel_id);
  context.call(rpc_request_buffer);
  ASSERT_EQ(active_drain.Flush(), OkStatus());
  active_drain.Close();
  ASSERT_EQ(context.status(), OkStatus());
  ASSERT_EQ(context.responses().size(), 1u);

  // Verify message.
  protobuf::Decoder entries_decoder(context.responses()[0]);
  ASSERT_TRUE(entries_decoder.Next().ok());
  ConstByteSpan entry;
  EXPECT_TRUE(entries_decoder.ReadBytes(&entry).ok());
  protobuf::Decoder entry_decoder(entry);
  VerifyLogEntry(entry_decoder, expected_entry);
}

TEST_F(LogServiceTest, InterruptedLogStreamSendsDropCount) {
  const uint32_t drain_channel_id = kCloseWriterOnErrorDrainId;
  auto drain = drain_map_.GetDrainFromChannelId(drain_channel_id);
  ASSERT_TRUE(drain.ok());

  LogService log_service(drain_map_);
  const size_t output_buffer_size = 128;
  const size_t max_packets = 10;
  rpc::RawFakeChannelOutput<10, output_buffer_size, 512> output;
  rpc::Channel channel(rpc::Channel::Create<drain_channel_id>(&output));
  rpc::Server server(std::span(&channel, 1));

  // Add as many entries needed to have multiple packets send.
  StatusWithSize status =
      AddLogEntry(kMessage, kSampleMetadata, kSampleTimestamp);
  ASSERT_TRUE(status.ok());

  // In reality less than output_buffer_size is given as a buffer, since some
  // bytes are used for the RPC framing.
  const uint32_t max_messages_per_response = output_buffer_size / status.size();
  // Send less packets than the max to avoid crashes.
  const uint32_t packets_sent = max_packets / 2;
  const size_t total_entries = packets_sent * max_messages_per_response;
  const size_t max_entries = 50;
  // Check we can test all these entries.
  ASSERT_GE(max_entries, total_entries);
  AddLogEntries(total_entries - 1, kMessage, kSampleMetadata, kSampleTimestamp);

  // Interrupt log stream with an error.
  const uint32_t successful_packets_sent = packets_sent / 2;
  output.set_send_status(Status::Unavailable(), successful_packets_sent);

  // Request logs.
  rpc::RawServerWriter writer = rpc::RawServerWriter::Open<Logs::Listen>(
      server, drain_channel_id, log_service);
  EXPECT_EQ(drain.value()->Open(writer), OkStatus());
  // This drain closes on errors.
  EXPECT_EQ(drain.value()->Flush(), Status::Aborted());
  EXPECT_TRUE(output.done());

  // Make sure not all packets were sent.
  ASSERT_EQ(output.payloads<Logs::Listen>().size(), successful_packets_sent);

  // Verify data in responses.
  Vector<TestLogEntry, max_entries> message_stack;
  for (size_t i = 0; i < total_entries; ++i) {
    message_stack.push_back({.timestamp = kSampleTimestamp,
                             .tokenized_data = std::as_bytes(
                                 std::span(std::string_view(kMessage)))});
  }
  size_t entries_found = 0;
  for (auto& response : output.payloads<Logs::Listen>()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += VerifyLogEntries(entry_decoder, message_stack);
  }

  // Verify that not all the entries were sent.
  EXPECT_LE(entries_found, total_entries);

  // Reset channel output and resume log stream with a new writer.
  output.clear();
  writer = rpc::RawServerWriter::Open<Logs::Listen>(
      server, drain_channel_id, log_service);
  EXPECT_EQ(drain.value()->Open(writer), OkStatus());
  EXPECT_EQ(drain.value()->Flush(), OkStatus());

  // Add expected messages to the stack in the reverse order they are received.
  message_stack.clear();
  // One full packet was dropped. Since all messages are the same length, there
  // are entries_found / successful_packets_sent per packet.
  const uint32_t total_drop_count = entries_found / successful_packets_sent;
  const uint32_t remaining_entries = total_entries - total_drop_count;
  for (size_t i = 0; i < remaining_entries; ++i) {
    message_stack.push_back({.tokenized_data = std::as_bytes(
                                 std::span(std::string_view(kMessage)))});
  }
  message_stack.push_back(
      {.metadata = kDropMessageMetadata, .dropped = total_drop_count});

  for (auto& response : output.payloads<Logs::Listen>()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += VerifyLogEntries(entry_decoder, message_stack);
  }
  // All entries are accounted for, including the drop message.
  EXPECT_EQ(entries_found, remaining_entries + 1);
}

TEST_F(LogServiceTest, InterruptedLogStreamIgnoresErrors) {
  const uint32_t drain_channel_id = kIgnoreWriterErrorsDrainId;
  auto drain = drain_map_.GetDrainFromChannelId(drain_channel_id);
  ASSERT_TRUE(drain.ok());

  LogService log_service(drain_map_);
  const size_t output_buffer_size = 50;
  const size_t max_packets = 20;
  rpc::RawFakeChannelOutput<max_packets, output_buffer_size, 512> output;
  rpc::Channel channel(rpc::Channel::Create<drain_channel_id>(&output));
  rpc::Server server(std::span(&channel, 1));

  // Add as many entries needed to have multiple packets send.
  StatusWithSize status =
      AddLogEntry(kMessage, kSampleMetadata, kSampleTimestamp);
  ASSERT_TRUE(status.ok());

  // In reality less than output_buffer_size is given as a buffer, since some
  // bytes are used for the RPC framing.
  const uint32_t max_messages_per_response = output_buffer_size / status.size();
  // Send less packets than the max to avoid crashes.
  const uint32_t packets_sent = 4;
  const size_t total_entries = packets_sent * max_messages_per_response;
  const size_t max_entries = 50;
  // Check we can test all these entries.q
  ASSERT_GE(max_entries, total_entries);
  AddLogEntries(total_entries - 1, kMessage, kSampleMetadata, kSampleTimestamp);

  // Interrupt log stream with an error.
  const uint32_t error_on_packet_count = packets_sent / 2;
  output.set_send_status(Status::Unavailable(), error_on_packet_count);

  // Request logs.
  rpc::RawServerWriter writer = rpc::RawServerWriter::Open<Logs::Listen>(
      server, drain_channel_id, log_service);
  EXPECT_EQ(drain.value()->Open(writer), OkStatus());
  // This drain ignores errors.
  EXPECT_EQ(drain.value()->Flush(), OkStatus());
  EXPECT_FALSE(output.done());

  // Make sure some packets were sent.
  ASSERT_GE(output.payloads<Logs::Listen>().size(), packets_sent);

  // Verify that not all the entries were sent.
  size_t entries_found = 0;
  for (auto& response : output.payloads<Logs::Listen>()) {
    protobuf::Decoder entry_decoder(response);
    entries_found += CountLogEntries(entry_decoder);
  }
  EXPECT_LE(entries_found, total_entries);

  // Verify that all messages were sent and the drop count messageis ignored.
  const uint32_t total_drop_count = total_entries - entries_found + 1;
  Vector<TestLogEntry, max_entries> message_stack;
  for (size_t i = 0; i < total_drop_count; ++i) {
    message_stack.push_back({.timestamp = kSampleTimestamp,
                             .tokenized_data = std::as_bytes(
                                 std::span(std::string_view(kMessage)))});
  }

  for (auto& response : output.payloads<Logs::Listen>()) {
    protobuf::Decoder entry_decoder(response);
    VerifyLogEntries(entry_decoder, message_stack);
  }

  // More calls to flush with errors will not affect this stubborn drain.
  const size_t previous_stream_packet_count =
      output.payloads<Logs::Listen>().size();
  output.set_send_status(Status::Unavailable());
  EXPECT_EQ(drain.value()->Flush(), OkStatus());
  EXPECT_FALSE(output.done());
  ASSERT_EQ(output.payloads<Logs::Listen>().size(),
            previous_stream_packet_count);

  output.clear();
  EXPECT_EQ(drain.value()->Close(), OkStatus());
  EXPECT_TRUE(output.done());
}

}  // namespace
}  // namespace pw::log_rpc
