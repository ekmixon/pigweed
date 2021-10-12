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

#include "pw_rpc/internal/client_call.h"

namespace pw::rpc::internal {

void ClientCall::SendInitialRequest(ConstByteSpan payload) {
  if (const Status status = SendPacket(PacketType::REQUEST, payload);
      !status.ok()) {
    HandleError(status);
  }
}

UnaryResponseClientCall& UnaryResponseClientCall::operator=(
    UnaryResponseClientCall&& other) {
  MoveFrom(other);
  on_completed_ = std::move(other.on_completed_);
  return *this;
}

StreamResponseClientCall& StreamResponseClientCall::operator=(
    StreamResponseClientCall&& other) {
  MoveFrom(other);
  on_completed_ = std::move(other.on_completed_);
  return *this;
}

}  // namespace pw::rpc::internal