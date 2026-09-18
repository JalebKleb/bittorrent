#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdio.h>

#define PROTOCOL_VERSION 1u
#define PROTOCOL_HEADER_SIZE 52u
#define PROTOCOL_MAX_PAYLOAD_SIZE (4u * 1024u * 1024u)

enum {
    PROTOCOL_OK = 0,
    PROTOCOL_ERR_IO = -1,
    PROTOCOL_ERR_CLOSED = -2,
    PROTOCOL_ERR_VERSION = -3,
    PROTOCOL_ERR_PAYLOAD_TOO_LARGE = -4,
    PROTOCOL_ERR_CHECKSUM = -5,
    PROTOCOL_ERR_MEMORY = -6,
    PROTOCOL_ERR_ARGUMENT = -7
};

typedef enum {
    MSG_JOIN = 1,
    MSG_LEAVE = 2,
    MSG_LOOKUP = 3,
    MSG_STORE = 4,
    MSG_DOWNLOAD_REQ = 5,
    MSG_DOWNLOAD_REP = 6,
    MSG_PREPARE = 7,
    MSG_COMMIT = 8,
    MSG_ABORT = 9,
    MSG_HEARTBEAT = 10,
    MSG_GOSSIP = 11,
    MSG_ELECTION = 12,
    MSG_OK = 13,
    MSG_COORDINATOR = 14,
    MSG_SNAPSHOT = 15,
    MSG_STATE_TRANSFER = 16,
    MSG_ACK = 17,
    MSG_ERROR = 18
} message_type_t;

typedef struct {
    uint16_t version;
    uint16_t type;
    uint64_t source_node;
    uint64_t destination_node;
    uint8_t transaction_id[16];
    uint64_t timestamp_ms;
    uint32_t payload_size;
    uint32_t checksum;
} protocol_header_t;

typedef struct {
    protocol_header_t header;
    uint8_t *payload;
} protocol_message_t;

uint64_t protocol_node_id_from_string(const char *text);
const char *protocol_message_type_name(uint16_t type);
const char *protocol_error_string(int code);

int protocol_send_message(int fd,
                          uint16_t type,
                          uint64_t source_node,
                          uint64_t destination_node,
                          const void *payload,
                          uint32_t payload_size);
int protocol_recv_message(int fd, protocol_message_t *out);
void protocol_free_message(protocol_message_t *message);

void protocol_transaction_id_hex(const uint8_t transaction_id[16],
                                 char output[33]);
void protocol_dump_header(FILE *stream, const protocol_header_t *header);

#endif
