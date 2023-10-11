/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: proto/toniebox.pb.rtnl.proto */

#ifndef PROTOBUF_C_proto_2ftoniebox_2epb_2ertnl_2eproto__INCLUDED
#define PROTOBUF_C_proto_2ftoniebox_2epb_2ertnl_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1000000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1003003 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct _TonieRtnlRPC TonieRtnlRPC;
typedef struct _TonieRtnlLog2 TonieRtnlLog2;
typedef struct _TonieRtnlLog3 TonieRtnlLog3;


/* --- enums --- */


/* --- messages --- */

struct  _TonieRtnlRPC
{
  ProtobufCMessage base;
  /*
   *required fixed64 length = 1;
   */
  TonieRtnlLog2 *log2;
  TonieRtnlLog3 *log3;
};
#define TONIE_RTNL_RPC__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&tonie_rtnl_rpc__descriptor) \
    , NULL, NULL }


struct  _TonieRtnlLog2
{
  ProtobufCMessage base;
  uint64_t uptime;
  uint32_t sequence;
  uint32_t field3;
  uint32_t function_group;
  uint32_t function;
  /*
   *or string
   */
  ProtobufCBinaryData field6;
  /*
   *optional <> field7 = 7;
   */
  protobuf_c_boolean has_field8;
  uint32_t field8;
  /*
   *or string
   */
  protobuf_c_boolean has_field9;
  ProtobufCBinaryData field9;
};
#define TONIE_RTNL_LOG2__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&tonie_rtnl_log2__descriptor) \
    , 0, 0, 0, 0, 0, {0,NULL}, 0, 0, 0, {0,NULL} }


struct  _TonieRtnlLog3
{
  ProtobufCMessage base;
  uint32_t datetime;
  uint32_t field2;
};
#define TONIE_RTNL_LOG3__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&tonie_rtnl_log3__descriptor) \
    , 0, 0 }


/* TonieRtnlRPC methods */
void   tonie_rtnl_rpc__init
                     (TonieRtnlRPC         *message);
size_t tonie_rtnl_rpc__get_packed_size
                     (const TonieRtnlRPC   *message);
size_t tonie_rtnl_rpc__pack
                     (const TonieRtnlRPC   *message,
                      uint8_t             *out);
size_t tonie_rtnl_rpc__pack_to_buffer
                     (const TonieRtnlRPC   *message,
                      ProtobufCBuffer     *buffer);
TonieRtnlRPC *
       tonie_rtnl_rpc__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   tonie_rtnl_rpc__free_unpacked
                     (TonieRtnlRPC *message,
                      ProtobufCAllocator *allocator);
/* TonieRtnlLog2 methods */
void   tonie_rtnl_log2__init
                     (TonieRtnlLog2         *message);
size_t tonie_rtnl_log2__get_packed_size
                     (const TonieRtnlLog2   *message);
size_t tonie_rtnl_log2__pack
                     (const TonieRtnlLog2   *message,
                      uint8_t             *out);
size_t tonie_rtnl_log2__pack_to_buffer
                     (const TonieRtnlLog2   *message,
                      ProtobufCBuffer     *buffer);
TonieRtnlLog2 *
       tonie_rtnl_log2__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   tonie_rtnl_log2__free_unpacked
                     (TonieRtnlLog2 *message,
                      ProtobufCAllocator *allocator);
/* TonieRtnlLog3 methods */
void   tonie_rtnl_log3__init
                     (TonieRtnlLog3         *message);
size_t tonie_rtnl_log3__get_packed_size
                     (const TonieRtnlLog3   *message);
size_t tonie_rtnl_log3__pack
                     (const TonieRtnlLog3   *message,
                      uint8_t             *out);
size_t tonie_rtnl_log3__pack_to_buffer
                     (const TonieRtnlLog3   *message,
                      ProtobufCBuffer     *buffer);
TonieRtnlLog3 *
       tonie_rtnl_log3__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   tonie_rtnl_log3__free_unpacked
                     (TonieRtnlLog3 *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*TonieRtnlRPC_Closure)
                 (const TonieRtnlRPC *message,
                  void *closure_data);
typedef void (*TonieRtnlLog2_Closure)
                 (const TonieRtnlLog2 *message,
                  void *closure_data);
typedef void (*TonieRtnlLog3_Closure)
                 (const TonieRtnlLog3 *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor tonie_rtnl_rpc__descriptor;
extern const ProtobufCMessageDescriptor tonie_rtnl_log2__descriptor;
extern const ProtobufCMessageDescriptor tonie_rtnl_log3__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_proto_2ftoniebox_2epb_2ertnl_2eproto__INCLUDED */