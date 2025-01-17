// Copyright 2020 The Pigweed Authors
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

#include <optional>

#include "gtest/gtest.h"
#include "pw_protobuf/decoder.h"
#include "pw_rpc/internal/hash.h"
#include "pw_rpc/raw/client_testing.h"
#include "pw_rpc/raw/test_method_context.h"
#include "pw_rpc_test_protos/test.pwpb.h"
#include "pw_rpc_test_protos/test.raw_rpc.pb.h"

namespace pw::rpc {
namespace {

Vector<std::byte, 64> EncodeRequest(int integer, Status status) {
  Vector<std::byte, 64> buffer(64);
  test::TestRequest::MemoryEncoder test_request(buffer);

  EXPECT_EQ(OkStatus(), test_request.WriteInteger(integer));
  EXPECT_EQ(OkStatus(), test_request.WriteStatusCode(status.code()));

  EXPECT_EQ(OkStatus(), test_request.status());
  buffer.resize(test_request.size());
  return buffer;
}

Vector<std::byte, 64> EncodeResponse(int number) {
  Vector<std::byte, 64> buffer(64);
  test::TestStreamResponse::MemoryEncoder test_response(buffer);

  EXPECT_EQ(OkStatus(), test_response.WriteNumber(number));

  EXPECT_EQ(OkStatus(), test_response.status());
  buffer.resize(test_response.size());
  return buffer;
}

}  // namespace

namespace test {

class TestService final : public generated::TestService<TestService> {
 public:
  static StatusWithSize TestUnaryRpc(ServerContext&,
                                     ConstByteSpan request,
                                     ByteSpan response) {
    int64_t integer;
    Status status;

    if (!DecodeRequest(request, integer, status)) {
      return StatusWithSize::DataLoss();
    }

    TestResponse::MemoryEncoder test_response(response);
    EXPECT_EQ(OkStatus(), test_response.WriteValue(integer + 1));

    return StatusWithSize(status, test_response.size());
  }

  static void TestAnotherUnaryRpc(ServerContext& ctx,
                                  ConstByteSpan request,
                                  RawUnaryResponder& responder) {
    ByteSpan response = responder.PayloadBuffer();
    StatusWithSize sws = TestUnaryRpc(ctx, request, response);
    responder.Finish(response.first(sws.size()), sws.status());
  }

  void TestServerStreamRpc(ServerContext&,
                           ConstByteSpan request,
                           RawServerWriter& writer) {
    int64_t integer;
    Status status;

    ASSERT_TRUE(DecodeRequest(request, integer, status));
    for (int i = 0; i < integer; ++i) {
      ByteSpan buffer = writer.PayloadBuffer();

      TestStreamResponse::MemoryEncoder test_stream_response(buffer);
      EXPECT_EQ(OkStatus(), test_stream_response.WriteNumber(i));
      EXPECT_EQ(OkStatus(), writer.Write(test_stream_response));
    }

    EXPECT_EQ(OkStatus(), writer.Finish(status));
  }

  void TestClientStreamRpc(ServerContext&, RawServerReader& reader) {
    last_reader_ = std::move(reader);

    last_reader_.set_on_next([this](ConstByteSpan payload) {
      EXPECT_EQ(OkStatus(),
                last_reader_.Finish(EncodeResponse(ReadInteger(payload)),
                                    Status::Unauthenticated()));
    });
  }

  void TestBidirectionalStreamRpc(ServerContext&,
                                  RawServerReaderWriter& reader_writer) {
    last_reader_writer_ = std::move(reader_writer);

    last_reader_writer_.set_on_next([this](ConstByteSpan payload) {
      EXPECT_EQ(
          OkStatus(),
          last_reader_writer_.Write(EncodeResponse(ReadInteger(payload))));
      EXPECT_EQ(OkStatus(), last_reader_writer_.Finish(Status::NotFound()));
    });
  }

 protected:
  RawServerReader last_reader_;
  RawServerReaderWriter last_reader_writer_;

 private:
  static uint32_t ReadInteger(ConstByteSpan request) {
    uint32_t integer = 0;

    protobuf::Decoder decoder(request);
    while (decoder.Next().ok()) {
      switch (static_cast<TestRequest::Fields>(decoder.FieldNumber())) {
        case TestRequest::Fields::INTEGER:
          EXPECT_EQ(OkStatus(), decoder.ReadUint32(&integer));
          break;
        case TestRequest::Fields::STATUS_CODE:
          break;
        default:
          ADD_FAILURE();
      }
    }

    return integer;
  }

  static bool DecodeRequest(ConstByteSpan request,
                            int64_t& integer,
                            Status& status) {
    protobuf::Decoder decoder(request);
    Status decode_status;
    bool has_integer = false;
    bool has_status = false;

    while (decoder.Next().ok()) {
      switch (static_cast<TestRequest::Fields>(decoder.FieldNumber())) {
        case TestRequest::Fields::INTEGER:
          decode_status = decoder.ReadInt64(&integer);
          EXPECT_EQ(OkStatus(), decode_status);
          has_integer = decode_status.ok();
          break;
        case TestRequest::Fields::STATUS_CODE: {
          uint32_t status_code;
          decode_status = decoder.ReadUint32(&status_code);
          EXPECT_EQ(OkStatus(), decode_status);
          has_status = decode_status.ok();
          status = static_cast<Status::Code>(status_code);
          break;
        }
      }
    }
    EXPECT_TRUE(has_integer);
    EXPECT_TRUE(has_status);
    return has_integer && has_status;
  }
};

}  // namespace test

namespace {

TEST(RawCodegen, Server_CompilesProperly) {
  test::TestService service;
  EXPECT_EQ(service.id(), internal::Hash("pw.rpc.test.TestService"));
  EXPECT_STREQ(service.name(), "TestService");
}

TEST(RawCodegen, Server_InvokeUnaryRpc) {
  PW_RAW_TEST_METHOD_CONTEXT(test::TestService, TestUnaryRpc) context;

  auto sws = context.call(EncodeRequest(123, OkStatus()));
  EXPECT_EQ(OkStatus(), sws.status());

  protobuf::Decoder decoder(context.response());

  while (decoder.Next().ok()) {
    switch (static_cast<test::TestResponse::Fields>(decoder.FieldNumber())) {
      case test::TestResponse::Fields::VALUE: {
        int32_t value;
        EXPECT_EQ(OkStatus(), decoder.ReadInt32(&value));
        EXPECT_EQ(value, 124);
        break;
      }
    }
  }
}

TEST(RawCodegen, Server_InvokeAsyncUnaryRpc) {
  PW_RAW_TEST_METHOD_CONTEXT(test::TestService, TestAnotherUnaryRpc) context;

  context.call(EncodeRequest(123, OkStatus()));
  EXPECT_EQ(OkStatus(), context.status());

  protobuf::Decoder decoder(context.response());

  while (decoder.Next().ok()) {
    switch (static_cast<test::TestResponse::Fields>(decoder.FieldNumber())) {
      case test::TestResponse::Fields::VALUE: {
        int32_t value;
        EXPECT_EQ(OkStatus(), decoder.ReadInt32(&value));
        EXPECT_EQ(value, 124);
        break;
      }
    }
  }
}

TEST(RawCodegen, Server_InvokeServerStreamingRpc) {
  PW_RAW_TEST_METHOD_CONTEXT(test::TestService, TestServerStreamRpc) context;

  context.call(EncodeRequest(5, Status::Unauthenticated()));
  EXPECT_TRUE(context.done());
  EXPECT_EQ(Status::Unauthenticated(), context.status());
  EXPECT_EQ(context.total_responses(), 5u);

  protobuf::Decoder decoder(context.responses().back());
  while (decoder.Next().ok()) {
    switch (
        static_cast<test::TestStreamResponse::Fields>(decoder.FieldNumber())) {
      case test::TestStreamResponse::Fields::NUMBER: {
        int32_t value;
        EXPECT_EQ(OkStatus(), decoder.ReadInt32(&value));
        EXPECT_EQ(value, 4);
        break;
      }
      case test::TestStreamResponse::Fields::CHUNK:
        FAIL();
        break;
    }
  }
}

int32_t ReadResponseNumber(ConstByteSpan data) {
  int32_t value = -1;
  protobuf::Decoder decoder(data);
  while (decoder.Next().ok()) {
    switch (
        static_cast<test::TestStreamResponse::Fields>(decoder.FieldNumber())) {
      case test::TestStreamResponse::Fields::NUMBER: {
        EXPECT_EQ(OkStatus(), decoder.ReadInt32(&value));
        break;
      }
      default:
        ADD_FAILURE();
        break;
    }
  }

  return value;
}

TEST(RawCodegen, Server_InvokeClientStreamingRpc) {
  PW_RAW_TEST_METHOD_CONTEXT(test::TestService, TestClientStreamRpc) ctx;

  ctx.call();
  ctx.SendClientStream(EncodeRequest(123, OkStatus()));

  ASSERT_TRUE(ctx.done());
  EXPECT_EQ(Status::Unauthenticated(), ctx.status());
  EXPECT_EQ(ctx.total_responses(), 1u);
  EXPECT_EQ(ReadResponseNumber(ctx.responses().back()), 123);
}

TEST(RawCodegen, Server_InvokeBidirectionalStreamingRpc) {
  PW_RAW_TEST_METHOD_CONTEXT(test::TestService, TestBidirectionalStreamRpc)
  ctx;

  ctx.call();
  ctx.SendClientStream(EncodeRequest(456, OkStatus()));

  ASSERT_TRUE(ctx.done());
  EXPECT_EQ(Status::NotFound(), ctx.status());
  ASSERT_EQ(ctx.total_responses(), 1u);
  EXPECT_EQ(ReadResponseNumber(ctx.responses().back()), 456);
}

TEST(RawCodegen, Client_ClientClass) {
  RawClientTestContext context;

  test::pw_rpc::raw::TestService::Client service_client(context.client(),
                                                        context.channel().id());

  EXPECT_EQ(service_client.channel_id(), context.channel().id());
  EXPECT_EQ(&service_client.client(), &context.client());
}

class RawCodegenClientTest : public ::testing::Test {
 protected:
  RawCodegenClientTest()
      : service_client_(context_.client(), context_.channel().id()) {}

  // Assumes the payload is a null-terminated string, not a protobuf.
  Function<void(ConstByteSpan)> OnNext() {
    return [this](ConstByteSpan c_string) { CopyPayload(c_string); };
  }

  Function<void(Status)> OnCompleted() {
    return [this](Status status) { status_ = status; };
  }

  Function<void(ConstByteSpan, Status)> UnaryOnCompleted() {
    return [this](ConstByteSpan c_string, Status status) {
      CopyPayload(c_string);
      status_ = status;
    };
  }

  Function<void(Status)> OnError() {
    return [this](Status error) { error_ = error; };
  }

  RawClientTestContext<> context_;

  test::pw_rpc::raw::TestService::Client service_client_;

  std::optional<const char*> payload_;
  std::optional<Status> status_;
  std::optional<Status> error_;

 private:
  void CopyPayload(ConstByteSpan c_string) {
    ASSERT_LE(c_string.size(), sizeof(buffer_));
    std::memcpy(buffer_, c_string.data(), c_string.size());
    payload_ = buffer_;
  }

  char buffer_[64];
};

TEST_F(RawCodegenClientTest, InvokeUnaryRpc_Ok) {
  RawUnaryReceiver call = test::pw_rpc::raw::TestService::TestUnaryRpc(
      context_.client(),
      context_.channel().id(),
      std::as_bytes(std::span("This is the request")),
      UnaryOnCompleted(),
      OnError());

  context_.server().SendResponse<test::pw_rpc::raw::TestService::TestUnaryRpc>(
      std::as_bytes(std::span("(ㆆ_ㆆ)")), OkStatus());

  ASSERT_TRUE(payload_.has_value());
  EXPECT_STREQ(payload_.value(), "(ㆆ_ㆆ)");
  EXPECT_EQ(status_, OkStatus());
  EXPECT_FALSE(error_.has_value());
}

TEST_F(RawCodegenClientTest, InvokeUnaryRpc_Error) {
  RawUnaryReceiver call = service_client_.TestUnaryRpc(
      std::as_bytes(std::span("This is the request")),
      UnaryOnCompleted(),
      OnError());

  context_.server()
      .SendServerError<test::pw_rpc::raw::TestService::TestUnaryRpc>(
          Status::NotFound());

  EXPECT_FALSE(payload_.has_value());
  EXPECT_FALSE(status_.has_value());
  EXPECT_EQ(error_, Status::NotFound());
}

TEST_F(RawCodegenClientTest, InvokeServerStreamRpc_Ok) {
  RawClientReader call = test::pw_rpc::raw::TestService::TestServerStreamRpc(
      context_.client(),
      context_.channel().id(),
      std::as_bytes(std::span("This is the request")),
      OnNext(),
      OnCompleted(),
      OnError());

  context_.server()
      .SendServerStream<test::pw_rpc::raw::TestService::TestServerStreamRpc>(
          std::as_bytes(std::span("(⌐□_□)")));

  ASSERT_TRUE(payload_.has_value());
  EXPECT_STREQ(payload_.value(), "(⌐□_□)");

  context_.server()
      .SendServerStream<test::pw_rpc::raw::TestService::TestServerStreamRpc>(
          std::as_bytes(std::span("(o_O)")));

  EXPECT_STREQ(payload_.value(), "(o_O)");

  context_.server()
      .SendResponse<test::pw_rpc::raw::TestService::TestServerStreamRpc>(
          Status::InvalidArgument());

  EXPECT_EQ(status_, Status::InvalidArgument());
  EXPECT_FALSE(error_.has_value());
}

TEST_F(RawCodegenClientTest, InvokeServerStreamRpc_Error) {
  RawClientReader call = service_client_.TestServerStreamRpc(
      std::as_bytes(std::span("This is the request")),
      OnNext(),
      OnCompleted(),
      OnError());

  context_.server()
      .SendServerError<test::pw_rpc::raw::TestService::TestServerStreamRpc>(
          Status::FailedPrecondition());

  EXPECT_FALSE(payload_.has_value());
  EXPECT_FALSE(status_.has_value());
  EXPECT_EQ(error_, Status::FailedPrecondition());
}

TEST_F(RawCodegenClientTest, InvokeClientStreamRpc_Ok) {
  RawClientWriter call = test::pw_rpc::raw::TestService::TestClientStreamRpc(
      context_.client(),
      context_.channel().id(),
      UnaryOnCompleted(),
      OnError());

  EXPECT_EQ(OkStatus(), call.Write(std::as_bytes(std::span("(•‿•)"))));
  EXPECT_STREQ(
      reinterpret_cast<const char*>(
          context_.output()
              .payloads<test::pw_rpc::raw::TestService::TestClientStreamRpc>()
              .back()
              .data()),
      "(•‿•)");

  context_.server()
      .SendResponse<test::pw_rpc::raw::TestService::TestClientStreamRpc>(
          std::as_bytes(std::span("(⌐□_□)")), Status::InvalidArgument());

  ASSERT_TRUE(payload_.has_value());
  EXPECT_STREQ(payload_.value(), "(⌐□_□)");
  EXPECT_EQ(status_, Status::InvalidArgument());
  EXPECT_FALSE(error_.has_value());
}

TEST_F(RawCodegenClientTest, InvokeClientStreamRpc_Error) {
  RawClientWriter call =
      service_client_.TestClientStreamRpc(UnaryOnCompleted(), OnError());

  context_.server()
      .SendServerError<test::pw_rpc::raw::TestService::TestClientStreamRpc>(
          Status::FailedPrecondition());

  EXPECT_FALSE(payload_.has_value());
  EXPECT_FALSE(status_.has_value());
  EXPECT_EQ(error_, Status::FailedPrecondition());
}

TEST_F(RawCodegenClientTest, InvokeBidirectionalStreamRpc_Ok) {
  RawClientReaderWriter call =
      test::pw_rpc::raw::TestService::TestBidirectionalStreamRpc(
          context_.client(),
          context_.channel().id(),
          OnNext(),
          OnCompleted(),
          OnError());

  EXPECT_EQ(OkStatus(), call.Write(std::as_bytes(std::span("(•‿•)"))));
  EXPECT_STREQ(
      reinterpret_cast<const char*>(
          context_.output()
              .payloads<
                  test::pw_rpc::raw::TestService::TestBidirectionalStreamRpc>()
              .back()
              .data()),
      "(•‿•)");

  context_.server()
      .SendServerStream<
          test::pw_rpc::raw::TestService::TestBidirectionalStreamRpc>(
          std::as_bytes(std::span("(⌐□_□)")));

  ASSERT_TRUE(payload_.has_value());
  EXPECT_STREQ(payload_.value(), "(⌐□_□)");

  context_.server()
      .SendResponse<test::pw_rpc::raw::TestService::TestBidirectionalStreamRpc>(
          Status::InvalidArgument());

  EXPECT_EQ(status_, Status::InvalidArgument());
  EXPECT_FALSE(error_.has_value());
}

TEST_F(RawCodegenClientTest, InvokeBidirectionalStreamRpc_Error) {
  RawClientReaderWriter call = service_client_.TestBidirectionalStreamRpc(
      OnNext(), OnCompleted(), OnError());

  context_.server()
      .SendServerError<
          test::pw_rpc::raw::TestService::TestBidirectionalStreamRpc>(
          Status::Internal());

  EXPECT_FALSE(payload_.has_value());
  EXPECT_FALSE(status_.has_value());
  EXPECT_EQ(error_, Status::Internal());
}

}  // namespace
}  // namespace pw::rpc
