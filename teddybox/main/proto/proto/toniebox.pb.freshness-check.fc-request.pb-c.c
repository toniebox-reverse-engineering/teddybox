/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: proto/toniebox.pb.freshness-check.fc-request.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "proto/toniebox.pb.freshness-check.fc-request.pb-c.h"
void   tonie_freshness_check_request__init
                     (TonieFreshnessCheckRequest         *message)
{
  static const TonieFreshnessCheckRequest init_value = TONIE_FRESHNESS_CHECK_REQUEST__INIT;
  *message = init_value;
}
size_t tonie_freshness_check_request__get_packed_size
                     (const TonieFreshnessCheckRequest *message)
{
  assert(message->base.descriptor == &tonie_freshness_check_request__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t tonie_freshness_check_request__pack
                     (const TonieFreshnessCheckRequest *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &tonie_freshness_check_request__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t tonie_freshness_check_request__pack_to_buffer
                     (const TonieFreshnessCheckRequest *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &tonie_freshness_check_request__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
TonieFreshnessCheckRequest *
       tonie_freshness_check_request__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (TonieFreshnessCheckRequest *)
     protobuf_c_message_unpack (&tonie_freshness_check_request__descriptor,
                                allocator, len, data);
}
void   tonie_freshness_check_request__free_unpacked
                     (TonieFreshnessCheckRequest *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &tonie_freshness_check_request__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   tonie_fcinfo__init
                     (TonieFCInfo         *message)
{
  static const TonieFCInfo init_value = TONIE_FCINFO__INIT;
  *message = init_value;
}
size_t tonie_fcinfo__get_packed_size
                     (const TonieFCInfo *message)
{
  assert(message->base.descriptor == &tonie_fcinfo__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t tonie_fcinfo__pack
                     (const TonieFCInfo *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &tonie_fcinfo__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t tonie_fcinfo__pack_to_buffer
                     (const TonieFCInfo *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &tonie_fcinfo__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
TonieFCInfo *
       tonie_fcinfo__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (TonieFCInfo *)
     protobuf_c_message_unpack (&tonie_fcinfo__descriptor,
                                allocator, len, data);
}
void   tonie_fcinfo__free_unpacked
                     (TonieFCInfo *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &tonie_fcinfo__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor tonie_freshness_check_request__field_descriptors[1] =
{
  {
    "tonie_infos",
    1,
    PROTOBUF_C_LABEL_REPEATED,
    PROTOBUF_C_TYPE_MESSAGE,
    offsetof(TonieFreshnessCheckRequest, n_tonie_infos),
    offsetof(TonieFreshnessCheckRequest, tonie_infos),
    &tonie_fcinfo__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned tonie_freshness_check_request__field_indices_by_name[] = {
  0,   /* field[0] = tonie_infos */
};
static const ProtobufCIntRange tonie_freshness_check_request__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 1 }
};
const ProtobufCMessageDescriptor tonie_freshness_check_request__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "TonieFreshnessCheckRequest",
  "TonieFreshnessCheckRequest",
  "TonieFreshnessCheckRequest",
  "",
  sizeof(TonieFreshnessCheckRequest),
  1,
  tonie_freshness_check_request__field_descriptors,
  tonie_freshness_check_request__field_indices_by_name,
  1,  tonie_freshness_check_request__number_ranges,
  (ProtobufCMessageInit) tonie_freshness_check_request__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor tonie_fcinfo__field_descriptors[2] =
{
  {
    "uid",
    1,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_FIXED64,
    0,   /* quantifier_offset */
    offsetof(TonieFCInfo, uid),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "audio_id",
    2,
    PROTOBUF_C_LABEL_REQUIRED,
    PROTOBUF_C_TYPE_FIXED32,
    0,   /* quantifier_offset */
    offsetof(TonieFCInfo, audio_id),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned tonie_fcinfo__field_indices_by_name[] = {
  1,   /* field[1] = audio_id */
  0,   /* field[0] = uid */
};
static const ProtobufCIntRange tonie_fcinfo__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 2 }
};
const ProtobufCMessageDescriptor tonie_fcinfo__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "TonieFCInfo",
  "TonieFCInfo",
  "TonieFCInfo",
  "",
  sizeof(TonieFCInfo),
  2,
  tonie_fcinfo__field_descriptors,
  tonie_fcinfo__field_indices_by_name,
  1,  tonie_fcinfo__number_ranges,
  (ProtobufCMessageInit) tonie_fcinfo__init,
  NULL,NULL,NULL    /* reserved[123] */
};
