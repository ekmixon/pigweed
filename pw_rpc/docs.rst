.. _module-pw_rpc:

------
pw_rpc
------
The ``pw_rpc`` module provides a system for defining and invoking remote
procedure calls (RPCs) on a device.

This document discusses the ``pw_rpc`` protocol and its C++ implementation.
``pw_rpc`` implementations for other languages are described in their own
documents:

.. toctree::
  :maxdepth: 1

  py/docs
  ts/docs

.. admonition:: Try it out!

  For a quick intro to ``pw_rpc``, see the
  :ref:`module-pw_hdlc-rpc-example` in the :ref:`module-pw_hdlc` module.

.. attention::

  This documentation is under construction.

RPC semantics
=============
The semantics of ``pw_rpc`` are similar to `gRPC
<https://grpc.io/docs/what-is-grpc/core-concepts/>`_.

RPC call lifecycle
------------------
In ``pw_rpc``, an RPC begins when the client sends a request packet. The server
receives the request, looks up the relevant service method, then calls into the
RPC function. The RPC is considered active until the server sends a response
packet with the RPC's status. The client may terminate an ongoing RPC by
cancelling it.

``pw_rpc`` supports only one RPC invocation per service/method/channel. If a
client calls an ongoing RPC on the same channel, the server cancels the ongoing
call and reinvokes the RPC with the new request. This applies to unary and
streaming RPCs, though the server may not have an opportunity to cancel a
synchronously handled unary RPC before it completes. The same RPC may be invoked
multiple times simultaneously if the invocations are on different channels.

Unrequested responses
---------------------
``pw_rpc`` supports sending responses to RPCs that have not yet been invoked by
a client. This is useful in testing and in situations like an RPC that triggers
reboot. After the reboot, the device opens the writer object and sends its
response to the client.

The C++ API for opening a server reader/writer takes the generated RPC function
as a template parameter. The server to use, channel ID, and service instance are
passed as arguments. The API is the same for all RPC types, except the
appropriate reader/writer class must be used.

.. code-block:: c++

  // Open a ServerWriter for a server streaming RPC.
  auto writer = RawServerWriter::Open<pw_rpc::raw::ServiceName::MethodName>(
      server, channel_id, service_instance);

  // Send some responses, even though the client has not yet called this RPC.
  CHECK_OK(writer.Write(encoded_response_1));
  CHECK_OK(writer.Write(encoded_response_2));

  // Finish the RPC.
  CHECK_OK(writer.Finish(OkStatus()));

Creating an RPC
===============

1. RPC service declaration
--------------------------
Pigweed RPCs are declared in a protocol buffer service definition.

* `Protocol Buffer service documentation
  <https://developers.google.com/protocol-buffers/docs/proto3#services>`_
* `gRPC service definition documentation
  <https://grpc.io/docs/what-is-grpc/core-concepts/#service-definition>`_

.. code-block:: protobuf

  syntax = "proto3";

  package foo.bar;

  message Request {}

  message Response {
    int32 number = 1;
  }

  service TheService {
    rpc MethodOne(Request) returns (Response) {}
    rpc MethodTwo(Request) returns (stream Response) {}
  }

This protocol buffer is declared in a ``BUILD.gn`` file as follows:

.. code-block:: python

  import("//build_overrides/pigweed.gni")
  import("$dir_pw_protobuf_compiler/proto.gni")

  pw_proto_library("the_service_proto") {
    sources = [ "foo_bar/the_service.proto" ]
  }

.. admonition:: proto2 or proto3 syntax?

  Always use proto3 syntax rather than proto2 for new protocol buffers. Proto2
  protobufs can be compiled for ``pw_rpc``, but they are not as well supported
  as proto3. Specifically, ``pw_rpc`` lacks support for non-zero default values
  in proto2. When using Nanopb with ``pw_rpc``, proto2 response protobufs with
  non-zero field defaults should be manually initialized to the default struct.

  In the past, proto3 was sometimes avoided because it lacked support for field
  presence detection. Fortunately, this has been fixed: proto3 now supports
  ``optional`` fields, which are equivalent to proto2 ``optional`` fields.

  If you need to distinguish between a default-valued field and a missing field,
  mark the field as ``optional``. The presence of the field can be detected
  with a ``HasField(name)`` or ``has_<field>`` member, depending on the library.

  Optional fields have some overhead --- default-valued fields are included in
  the encoded proto, and, if using Nanopb, the proto structs have a
  ``has_<field>`` flag for each optional field. Use plain fields if field
  presence detection is not needed.

  .. code-block:: protobuf

    syntax = "proto3";

    message MyMessage {
      // Leaving this field unset is equivalent to setting it to 0.
      int32 number = 1;

      // Setting this field to 0 is different from leaving it unset.
      optional int32 other_number = 2;
    }

2. RPC code generation
----------------------
``pw_rpc`` generates a C++ header file for each ``.proto`` file. This header is
generated in the build output directory. Its exact location varies by build
system and toolchain, but the C++ include path always matches the sources
declaration in the ``pw_proto_library``. The ``.proto`` extension is replaced
with an extension corresponding to the protobuf library in use.

================== =============== =============== =============
Protobuf libraries Build subtarget Protobuf header pw_rpc header
================== =============== =============== =============
Raw only           .raw_rpc        (none)          .raw_rpc.pb.h
Nanopb or raw      .nanopb_rpc     .pb.h           .rpc.pb.h
pw_protobuf or raw .pwpb_rpc       .pwpb.h         .rpc.pwpb.h
================== =============== =============== =============

For example, the generated RPC header for ``"foo_bar/the_service.proto"`` is
``"foo_bar/the_service.rpc.pb.h"`` for Nanopb or
``"foo_bar/the_service.raw_rpc.pb.h"`` for raw RPCs.

The generated header defines a base class for each RPC service declared in the
``.proto`` file. A service named ``TheService`` in package ``foo.bar`` would
generate the following base class for Nanopb:

.. cpp:class:: template <typename Implementation> foo::bar::pw_rpc::nanopb::TheService::Service

3. RPC service definition
-------------------------
The serivce class is implemented by inheriting from the generated RPC service
base class and defining a method for each RPC. The methods must match the name
and function signature for one of the supported protobuf implementations.
Services may mix and match protobuf implementations within one service.

.. tip::

  The generated code includes RPC service implementation stubs. You can
  reference or copy and paste these to get started with implementing a service.
  These stub classes are generated at the bottom of the pw_rpc proto header.

  To use the stubs, do the following:

  #. Locate the generated RPC header in the build directory. For example:

     .. code-block:: sh

       find out/ -name <proto_name>.rpc.pb.h

  #. Scroll to the bottom of the generated RPC header.
  #. Copy the stub class declaration to a header file.
  #. Copy the member function definitions to a source file.
  #. Rename the class or change the namespace, if desired.
  #. List these files in a build target with a dependency on the
     ``pw_proto_library``.

A Nanopb implementation of this service would be as follows:

.. code-block:: cpp

  #include "foo_bar/the_service.rpc.pb.h"

  namespace foo::bar {

  class TheService : public pw_rpc::nanopb::TheService::Service<TheService> {
   public:
    pw::Status MethodOne(ServerContext&,
                         const foo_bar_Request& request,
                         foo_bar_Response& response) {
      // implementation
      return pw::OkStatus();
    }

    void MethodTwo(ServerContext&,
                   const foo_bar_Request& request,
                   ServerWriter<foo_bar_Response>& response) {
      // implementation
      response.Write(foo_bar_Response{.number = 123});
    }
  };

  }  // namespace foo::bar

The Nanopb implementation would be declared in a ``BUILD.gn``:

.. code-block:: python

  import("//build_overrides/pigweed.gni")

  import("$dir_pw_build/target_types.gni")

  pw_source_set("the_service") {
    public_configs = [ ":public" ]
    public = [ "public/foo_bar/service.h" ]
    public_deps = [ ":the_service_proto.nanopb_rpc" ]
  }

.. attention::

  pw_rpc's generated classes will support using ``pw_protobuf`` or raw buffers
  (no protobuf library) in the future.

4. Register the service with a server
-------------------------------------
This example code sets up an RPC server with an :ref:`HDLC<module-pw_hdlc>`
channel output and the example service.

.. code-block:: cpp

  // Set up the output channel for the pw_rpc server to use. This configures the
  // pw_rpc server to use HDLC over UART; projects not using UART and HDLC must
  // adapt this as necessary.
  pw::stream::SysIoWriter writer;
  pw::rpc::RpcChannelOutput<kMaxTransmissionUnit> hdlc_channel_output(
      writer, pw::hdlc::kDefaultRpcAddress, "HDLC output");

  pw::rpc::Channel channels[] = {
      pw::rpc::Channel::Create<1>(&hdlc_channel_output)};

  // Declare the pw_rpc server with the HDLC channel.
  pw::rpc::Server server(channels);

  pw::rpc::TheService the_service;

  void RegisterServices() {
    // Register the foo.bar.TheService example service.
    server.Register(the_service);

    // Register other services
  }

  int main() {
    // Set up the server.
    RegisterServices();

    // Declare a buffer for decoding incoming HDLC frames.
    std::array<std::byte, kMaxTransmissionUnit> input_buffer;

    PW_LOG_INFO("Starting pw_rpc server");
    pw::hdlc::ReadAndProcessPackets(
        server, hdlc_channel_output, input_buffer);
  }

Channels
========
``pw_rpc`` sends all of its packets over channels. These are logical,
application-layer routes used to tell the RPC system where a packet should go.

Channels over a client-server connection must all have a unique ID, which can be
assigned statically at compile time or dynamically.

.. code-block:: cpp

  // Creating a channel with the static ID 3.
  pw::rpc::Channel static_channel = pw::rpc::Channel::Create<3>(&output);

  // Grouping channel IDs within an enum can lead to clearer code.
  enum ChannelId {
    kUartChannel = 1,
    kSpiChannel = 2,
  };

  // Creating a channel with a static ID defined within an enum.
  pw::rpc::Channel another_static_channel =
      pw::rpc::Channel::Create<ChannelId::kUartChannel>(&output);

  // Creating a channel with a dynamic ID (note that no output is provided; it
  // will be set when the channel is used.
  pw::rpc::Channel dynamic_channel;

Sometimes, the ID and output of a channel are not known at compile time as they
depend on information stored on the physical device. To support this use case, a
dynamically-assignable channel can be configured once at runtime with an ID and
output.

.. code-block:: cpp

  // Create a dynamic channel without a compile-time ID or output.
  pw::rpc::Channel dynamic_channel;

  void Init() {
    // Called during boot to pull the channel configuration from the system.
    dynamic_channel.Configure(GetChannelId(), some_output);
  }


Services
========
A service is a logical grouping of RPCs defined within a .proto file. ``pw_rpc``
uses these .proto definitions to generate code for a base service, from which
user-defined RPCs are implemented.

``pw_rpc`` supports multiple protobuf libraries, and the generated code API
depends on which is used.

.. _module-pw_rpc-protobuf-library-apis:

Protobuf library APIs
=====================

.. toctree::
  :maxdepth: 1

  nanopb/docs

Testing a pw_rpc integration
============================
After setting up a ``pw_rpc`` server in your project, you can test that it is
working as intended by registering the provided ``EchoService``, defined in
``echo.proto``, which echoes back a message that it receives.

.. literalinclude:: echo.proto
  :language: protobuf
  :lines: 14-

For example, in C++ with nanopb:

.. code:: c++

  #include "pw_rpc/server.h"

  // Include the apporpriate header for your protobuf library.
  #include "pw_rpc/echo_service_nanopb.h"

  constexpr pw::rpc::Channel kChannels[] = { /* ... */ };
  static pw::rpc::Server server(kChannels);

  static pw::rpc::EchoService echo_service;

  void Init() {
    server.RegisterService(echo_service);
  }

Benchmarking and stress testing
-------------------------------

.. toctree::
  :maxdepth: 1
  :hidden:

  benchmark

``pw_rpc`` provides an RPC service and Python module for stress testing and
benchmarking a ``pw_rpc`` deployment. See :ref:`module-pw_rpc-benchmark`.

Naming
======

Reserved names
--------------
``pw_rpc`` reserves a few service method names so they can be used for generated
classes. The following names cannnot be used for service methods:

- ``Client``
- ``Service``
- Any reserved words in the languages ``pw_rpc`` supports (e.g. ``class``).

``pw_rpc`` does not reserve any service names, but the restriction of avoiding
reserved words in supported languages applies.

Service naming style
--------------------
``pw_rpc`` service names should use capitalized camel case and should not use
the term "Service". Appending "Service" to a service name is redundant, similar
to appending "Class" or "Function" to a class or function name. The
C++ implementation class may use "Service" in its name, however.

For example, a service for accessing a file system should simply be named
``service FileSystem``, rather than ``service FileSystemService``, in the
``.proto`` file.

.. code-block:: protobuf

  // file.proto
  package pw.file;

  service FileSystem {
      rpc List(ListRequest) returns (stream ListResponse);
  }

The C++ service implementation class may append "Service" to the name.

.. code-block:: cpp

  // file_system_service.h
  #include "pw_file/file.raw_rpc.pb.h"

  namespace pw::file {

  class FileSystemService : public pw_rpc::raw::FileSystem::Service<FileSystemService> {
    void List(ServerContext&, ConstByteSpan request, RawServerWriter& writer);
  };

  }

For upstream Pigweed services, this naming style is a requirement. Note that
some services created before this was established may use non-compliant
names. For Pigweed users, this naming style is a suggestion.

Protocol description
====================
Pigweed RPC servers and clients communicate using ``pw_rpc`` packets. These
packets are used to send requests and responses, control streams, cancel ongoing
RPCs, and report errors.

Packet format
-------------
Pigweed RPC packets consist of a type and a set of fields. The packets are
encoded as protocol buffers. The full packet format is described in
``pw_rpc/pw_rpc/internal/packet.proto``.

.. literalinclude:: internal/packet.proto
  :language: protobuf
  :lines: 14-

The packet type and RPC type determine which fields are present in a Pigweed RPC
packet. Each packet type is only sent by either the client or the server.
These tables describe the meaning of and fields included with each packet type.

Client-to-server packets
^^^^^^^^^^^^^^^^^^^^^^^^
+-------------------+-------------------------------------+
| packet type       | description                         |
+===================+=====================================+
| REQUEST           | Invoke an RPC                       |
|                   |                                     |
|                   | .. code-block:: text                |
|                   |                                     |
|                   |   - channel_id                      |
|                   |   - service_id                      |
|                   |   - method_id                       |
|                   |   - payload                         |
|                   |     (unary & server streaming only) |
|                   |   - call_id (optional)              |
|                   |                                     |
+-------------------+-------------------------------------+
| CLIENT_STREAM     | Message in a client stream          |
|                   |                                     |
|                   | .. code-block:: text                |
|                   |                                     |
|                   |   - channel_id                      |
|                   |   - service_id                      |
|                   |   - method_id                       |
|                   |   - payload                         |
|                   |   - call_id (if set in REQUEST)     |
|                   |                                     |
+-------------------+-------------------------------------+
| CLIENT_STREAM_END | Client stream is complete           |
|                   |                                     |
|                   | .. code-block:: text                |
|                   |                                     |
|                   |   - channel_id                      |
|                   |   - service_id                      |
|                   |   - method_id                       |
|                   |   - call_id (if set in REQUEST)     |
|                   |                                     |
+-------------------+-------------------------------------+
| CLIENT_ERROR      | Abort an ongoing RPC                |
|                   |                                     |
|                   | .. code-block:: text                |
|                   |                                     |
|                   |   - channel_id                      |
|                   |   - service_id                      |
|                   |   - method_id                       |
|                   |   - status                          |
|                   |   - call_id (if set in REQUEST)     |
|                   |                                     |
+-------------------+-------------------------------------+

**Client errors**

The client sends ``CLIENT_ERROR`` packets to a server when it receives a packet
it did not request. If possible, the server should abort it.

The status code indicates the type of error. The status code is logged, but all
status codes result in the same action by the server: aborting the RPC.

* ``CANCELLED`` -- The client requested that the RPC be cancelled.
* ``NOT_FOUND`` -- Received a packet for a service method the client does not
  recognize.
* ``FAILED_PRECONDITION`` -- Received a packet for a service method that the
  client did not invoke.
* ``DATA_LOSS`` -- Received a corrupt packet for a pending service method.
* ``INVALID_ARGUMENT`` -- The server sent a packet type to an RPC that does not
  support it (a ``SERVER_STREAM`` was sent to an RPC with no server stream).

Server-to-client packets
^^^^^^^^^^^^^^^^^^^^^^^^
+-------------------+-------------------------------------+
| packet type       | description                         |
+===================+=====================================+
| RESPONSE          | The RPC is complete                 |
|                   |                                     |
|                   | .. code-block:: text                |
|                   |                                     |
|                   |   - channel_id                      |
|                   |   - service_id                      |
|                   |   - method_id                       |
|                   |   - status                          |
|                   |   - payload                         |
|                   |     (unary & client streaming only) |
|                   |   - call_id (if set in REQUEST)     |
|                   |                                     |
+-------------------+-------------------------------------+
| SERVER_STREAM     | Message in a server stream          |
|                   |                                     |
|                   | .. code-block:: text                |
|                   |                                     |
|                   |   - channel_id                      |
|                   |   - service_id                      |
|                   |   - method_id                       |
|                   |   - payload                         |
|                   |   - call_id (if set in REQUEST)     |
|                   |                                     |
+-------------------+-------------------------------------+
| SERVER_ERROR      | Received unexpected packet          |
|                   |                                     |
|                   | .. code-block:: text                |
|                   |                                     |
|                   |   - channel_id                      |
|                   |   - service_id (if relevant)        |
|                   |   - method_id (if relevant)         |
|                   |   - status                          |
|                   |   - call_id (if set in REQUEST)     |
|                   |                                     |
+-------------------+-------------------------------------+

All server packets contain the same client ID that was set in the initial
request made by the client, if any.

**Server errors**

The server sends ``SERVER_ERROR`` packets when it receives a packet it cannot
process. The client should abort any RPC for which it receives an error. The
status field indicates the type of error.

* ``NOT_FOUND`` -- The requested service or method does not exist.
* ``FAILED_PRECONDITION`` -- A client stream or cancel packet was sent for an
  RPC that is not pending.
* ``INVALID_ARGUMENT`` -- The client sent a packet type to an RPC that does not
  support it (a ``CLIENT_STREAM`` was sent to an RPC with no client stream).
* ``RESOURCE_EXHAUSTED`` -- The request came on a new channel, but a channel
  could not be allocated for it.
* ``INTERNAL`` -- The server was unable to respond to an RPC due to an
  unrecoverable internal error.

Inovking a service method
-------------------------
Calling an RPC requires a specific sequence of packets. This section describes
the protocol for calling service methods of each type: unary, server streaming,
client streaming, and bidirectional streaming.

The basic flow for all RPC invocations is as follows:

  * Client sends a ``REQUEST`` packet. Includes a payload for unary & server
    streaming RPCs.
  * For client and bidirectional streaming RPCs, the client may send any number
    of ``CLIENT_STREAM`` packets with payloads.
  * For server and bidirectional streaming RPCs, the server may send any number
    of ``SERVER_STREAM`` packets.
  * The server sends a ``RESPONSE`` packet. Includes a payload for unary &
    client streaming RPCs. The RPC is complete.

The client may cancel an ongoing RPC at any time by sending a ``CLIENT_ERROR``
packet with status ``CANCELLED``. The server may finish an ongoing RPC at any
time by sending the ``RESPONSE`` packet.

Unary RPC
^^^^^^^^^
In a unary RPC, the client sends a single request and the server sends a single
response.

.. image:: unary_rpc.svg

The client may attempt to cancel a unary RPC by sending a ``CLIENT_ERROR``
packet with status ``CANCELLED``. The server sends no response to a cancelled
RPC. If the server processes the unary RPC synchronously (the handling thread
sends the response), it may not be possible to cancel the RPC.

.. image:: unary_rpc_cancelled.svg

Server streaming RPC
^^^^^^^^^^^^^^^^^^^^
In a server streaming RPC, the client sends a single request and the server
sends any number of ``SERVER_STREAM`` packets followed by a ``RESPONSE`` packet.

.. image:: server_streaming_rpc.svg

The client may terminate a server streaming RPC by sending a ``CLIENT_STREAM``
packet with status ``CANCELLED``. The server sends no response.

.. image:: server_streaming_rpc_cancelled.svg

Client streaming RPC
^^^^^^^^^^^^^^^^^^^^
In a client streaming RPC, the client starts the RPC by sending a ``REQUEST``
packet with no payload. It then sends any number of messages in
``CLIENT_STREAM`` packets, followed by a ``CLIENT_STREAM_END``. The server sends
a single ``RESPONSE`` to finish the RPC.

.. image:: client_streaming_rpc.svg

The server may finish the RPC at any time by sending its ``RESPONSE`` packet,
even if it has not yet received the ``CLIENT_STREAM_END`` packet. The client may
terminate the RPC at any time by sending a ``CLIENT_ERROR`` packet with status
``CANCELLED``.

.. image:: client_streaming_rpc_cancelled.svg

Bidirectional streaming RPC
^^^^^^^^^^^^^^^^^^^^^^^^^^^
In a bidirectional streaming RPC, the client sends any number of requests and
the server sends any number of responses. The client invokes the RPC by sending
a ``REQUEST`` with no payload. It sends a ``CLIENT_STREAM_END`` packet when it
has finished sending requests. The server sends a ``RESPONSE`` packet to finish
the RPC.

.. image:: bidirectional_streaming_rpc.svg

The server may finish the RPC at any time by sending the ``RESPONSE`` packet,
even if it has not received the ``CLIENT_STREAM_END`` packet. The client may
terminate the RPC at any time by sending a ``CLIENT_ERROR`` packet with status
``CANCELLED``.

.. image:: bidirectional_streaming_rpc_cancelled.svg

RPC server
==========
Declare an instance of ``rpc::Server`` and register services with it.

.. admonition:: TODO

  Document the public interface

Size report
-----------
The following size report showcases the memory usage of the core RPC server. It
is configured with a single channel using a basic transport interface that
directly reads from and writes to ``pw_sys_io``. The transport has a 128-byte
packet buffer, which comprises the plurality of the example's RAM usage. This is
not a suitable transport for an actual product; a real implementation would have
additional overhead proportional to the complexity of the transport.

.. include:: server_size

RPC server implementation
-------------------------

The Method class
^^^^^^^^^^^^^^^^
The RPC Server depends on the ``pw::rpc::internal::Method`` class. ``Method``
serves as the bridge between the ``pw_rpc`` server library and the user-defined
RPC functions. Each supported protobuf implementation extends ``Method`` to
implement its request and response proto handling. The ``pw_rpc`` server
calls into the ``Method`` implementation through the base class's ``Invoke``
function.

``Method`` implementations store metadata about each method, including a
function pointer to the user-defined method implementation. They also provide
``static constexpr`` functions for creating each type of method. ``Method``
implementations must satisfy the ``MethodImplTester`` test class in
``pw_rpc/internal/method_impl_tester.h``.

See ``pw_rpc/internal/method.h`` for more details about ``Method``.

Packet flow
^^^^^^^^^^^

Requests
~~~~~~~~

.. image:: request_packets.svg

Responses
~~~~~~~~~

.. image:: response_packets.svg

RPC client
==========
The RPC client is used to send requests to a server and manages the contexts of
ongoing RPCs.

Setting up a client
-------------------
The ``pw::rpc::Client`` class is instantiated with a list of channels that it
uses to communicate. These channels can be shared with a server, but multiple
clients cannot use the same channels.

To send incoming RPC packets from the transport layer to be processed by a
client, the client's ``ProcessPacket`` function is called with the packet data.

.. code:: c++

  #include "pw_rpc/client.h"

  namespace {

  pw::rpc::Channel my_channels[] = {
      pw::rpc::Channel::Create<1>(&my_channel_output)};
  pw::rpc::Client my_client(my_channels);

  }  // namespace

  // Called when the transport layer receives an RPC packet.
  void ProcessRpcPacket(ConstByteSpan packet) {
    my_client.ProcessPacket(packet);
  }

.. _module-pw_rpc-making-calls:

Making RPC calls
----------------
RPC calls are not made directly through the client, but using one of its
registered channels instead. A service client class is generated from a .proto
file for each selected protobuf library, which is then used to send RPC requests
through a given channel. The API for this depends on the protobuf library;
please refer to the
:ref:`appropriate documentation<module-pw_rpc-protobuf-library-apis>`. Multiple
service client implementations can exist simulatenously and share the same
``Client`` class.

When a call is made, a ``pw::rpc::ClientCall`` object is returned to the caller.
This object tracks the ongoing RPC call, and can be used to manage it. An RPC
call is only active as long as its ``ClientCall`` object is alive.

.. tip::
  Use ``std::move`` when passing around ``ClientCall`` objects to keep RPCs
  alive.

Example
^^^^^^^
.. code-block:: c++

  #include "pw_rpc/echo_service_nanopb.h"

  namespace {
  // Generated clients are namespaced with their proto library.
  using pw::rpc::nanopb::EchoServiceClient;

  // RPC channel ID on which to make client calls.
  constexpr uint32_t kDefaultChannelId = 1;

  EchoServiceClient::EchoCall echo_call;

  // Callback invoked when a response is received. This is called synchronously
  // from Client::ProcessPacket.
  void EchoResponse(const pw_rpc_EchoMessage& response,
                    pw::Status status) {
    if (status.ok()) {
      PW_LOG_INFO("Received echo response: %s", response.msg);
    } else {
      PW_LOG_ERROR("Echo failed with status %d",
                   static_cast<int>(status.code()));
    }
  }

  }  // namespace

  void CallEcho(const char* message) {
    // Create a client to call the EchoService.
    EchoServiceClient echo_client(my_rpc_client, kDefaultChannelId);

    pw_rpc_EchoMessage request = pw_rpc_EchoMessage_init_default;
    pw::string::Copy(message, request.msg);

    // By assigning the returned ClientCall to the global echo_call, the RPC
    // call is kept alive until it completes. When a response is received, it
    // will be logged by the handler function and the call will complete.
    echo_call = echo_client.Echo(request, EchoResponse);
    if (!echo_call.active()) {
      // The RPC call was not sent. This could occur due to, for example, an
      // invalid channel ID. Handle if necessary.
    }
  }

Client implementation details
-----------------------------

The ClientCall class
^^^^^^^^^^^^^^^^^^^^
``ClientCall`` stores the context of an active RPC, and serves as the user's
interface to the RPC client. The core RPC library provides a base ``ClientCall``
class with common functionality, which is then extended for RPC client
implementations tied to different protobuf libraries to provide convenient
interfaces for working with RPCs.

The RPC server stores a list of all of active ``ClientCall`` objects. When an
incoming packet is recieved, it dispatches to one of its active calls, which
then decodes the payload and presents it to the user.

ClientServer
============
Sometimes, a device needs to both process RPCs as a server, as well as making
calls to another device as a client. To do this, both a client and server must
be set up, and incoming packets must be sent to both of them.

Pigweed simplifies this setup by providing a ``ClientServer`` class which wraps
an RPC client and server with the same set of channels.

.. code-block:: cpp

  pw::rpc::Channel channels[] = {
      pw::rpc::Channel::Create<1>(&channel_output)};

  // Creates both a client and a server.
  pw::rpc::ClientServer client_server(channels);

  void ProcessRpcData(pw::ConstByteSpan packet) {
    // Calls into both the client and the server, sending the packet to the
    // appropriate one.
    client_server.ProcessPacket(packet, output);
  }
