/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: statsmessage.proto */

#ifndef PROTOBUF_C_statsmessage_2eproto__INCLUDED
#define PROTOBUF_C_statsmessage_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1000000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1000000 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct _StatsRequest StatsRequest;
typedef struct _MsgCell MsgCell;
typedef struct _StatsReply StatsReply;


/* --- enums --- */


/* --- messages --- */

struct  _StatsRequest
{
  ProtobufCMessage base;
  char *key;
  char *view;
  protobuf_c_boolean has_topn;
  int32_t topn;
};
#define STATS_REQUEST__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&stats_request__descriptor) \
    , NULL, NULL, 0,0 }


struct  _MsgCell
{
  ProtobufCMessage base;
  char *name;
  protobuf_c_boolean has_count;
  int32_t count;
};
#define MSG_CELL__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&msg_cell__descriptor) \
    , NULL, 0,0 }


struct  _StatsReply
{
  ProtobufCMessage base;
  char *timestr;
  char *key;
  char *maybe;
  protobuf_c_boolean has_value;
  float value;
  size_t n_result;
  MsgCell **result;
  size_t n_name;
  char **name;
  size_t n_count;
  int32_t *count;
  size_t n_rate;
  int32_t *rate;
  size_t n_access_count;
  int32_t *access_count;
  size_t n_bandwidth;
  int32_t *bandwidth;
  size_t n_hit_count;
  int32_t *hit_count;
  size_t n_hit_bandwidth;
  int32_t *hit_bandwidth;
  size_t n_lost_count;
  int32_t *lost_count;
  size_t n_lost_bandwidth;
  int32_t *lost_bandwidth;
};
#define STATS_REPLY__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&stats_reply__descriptor) \
    , NULL, NULL, NULL, 0,0, 0,NULL, 0,NULL, 0,NULL, 0,NULL, 0,NULL, 0,NULL, 0,NULL, 0,NULL, 0,NULL, 0,NULL }


/* StatsRequest methods */
void   stats_request__init
                     (StatsRequest         *message);
size_t stats_request__get_packed_size
                     (const StatsRequest   *message);
size_t stats_request__pack
                     (const StatsRequest   *message,
                      uint8_t             *out);
size_t stats_request__pack_to_buffer
                     (const StatsRequest   *message,
                      ProtobufCBuffer     *buffer);
StatsRequest *
       stats_request__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   stats_request__free_unpacked
                     (StatsRequest *message,
                      ProtobufCAllocator *allocator);
/* MsgCell methods */
void   msg_cell__init
                     (MsgCell         *message);
size_t msg_cell__get_packed_size
                     (const MsgCell   *message);
size_t msg_cell__pack
                     (const MsgCell   *message,
                      uint8_t             *out);
size_t msg_cell__pack_to_buffer
                     (const MsgCell   *message,
                      ProtobufCBuffer     *buffer);
MsgCell *
       msg_cell__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   msg_cell__free_unpacked
                     (MsgCell *message,
                      ProtobufCAllocator *allocator);
/* StatsReply methods */
void   stats_reply__init
                     (StatsReply         *message);
size_t stats_reply__get_packed_size
                     (const StatsReply   *message);
size_t stats_reply__pack
                     (const StatsReply   *message,
                      uint8_t             *out);
size_t stats_reply__pack_to_buffer
                     (const StatsReply   *message,
                      ProtobufCBuffer     *buffer);
StatsReply *
       stats_reply__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   stats_reply__free_unpacked
                     (StatsReply *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*StatsRequest_Closure)
                 (const StatsRequest *message,
                  void *closure_data);
typedef void (*MsgCell_Closure)
                 (const MsgCell *message,
                  void *closure_data);
typedef void (*StatsReply_Closure)
                 (const StatsReply *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor stats_request__descriptor;
extern const ProtobufCMessageDescriptor msg_cell__descriptor;
extern const ProtobufCMessageDescriptor stats_reply__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_statsmessage_2eproto__INCLUDED */
