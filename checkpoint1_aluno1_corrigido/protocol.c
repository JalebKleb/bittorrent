#include "protocol.h"

#include "network.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OFFSET_VERSION 0u
#define OFFSET_TYPE 2u
#define OFFSET_SOURCE 4u
#define OFFSET_DESTINATION 12u
#define OFFSET_TRANSACTION 20u
#define OFFSET_TIMESTAMP 36u
#define OFFSET_PAYLOAD_SIZE 44u
#define OFFSET_CHECKSUM 48u

static pthread_mutex_t tx_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t tx_sequence = 0;

static uint64_t current_time_ms(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return 0;
    }

    return ((uint64_t)now.tv_sec * 1000u) + ((uint64_t)now.tv_nsec / 1000000u);
}

static void write_u16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)((value >> 8) & 0xffu);
    out[1] = (uint8_t)(value & 0xffu);
}

static void write_u32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)((value >> 24) & 0xffu);
    out[1] = (uint8_t)((value >> 16) & 0xffu);
    out[2] = (uint8_t)((value >> 8) & 0xffu);
    out[3] = (uint8_t)(value & 0xffu);
}

static void write_u64(uint8_t *out, uint64_t value)
{
    out[0] = (uint8_t)((value >> 56) & 0xffu);
    out[1] = (uint8_t)((value >> 48) & 0xffu);
    out[2] = (uint8_t)((value >> 40) & 0xffu);
    out[3] = (uint8_t)((value >> 32) & 0xffu);
    out[4] = (uint8_t)((value >> 24) & 0xffu);
    out[5] = (uint8_t)((value >> 16) & 0xffu);
    out[6] = (uint8_t)((value >> 8) & 0xffu);
    out[7] = (uint8_t)(value & 0xffu);
}

static uint16_t read_u16(const uint8_t *in)
{
    return (uint16_t)(((uint16_t)in[0] << 8) | (uint16_t)in[1]);
}

static uint32_t read_u32(const uint8_t *in)
{
    return ((uint32_t)in[0] << 24) |
           ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) |
           (uint32_t)in[3];
}

static uint64_t read_u64(const uint8_t *in)
{
    return ((uint64_t)in[0] << 56) |
           ((uint64_t)in[1] << 48) |
           ((uint64_t)in[2] << 40) |
           ((uint64_t)in[3] << 32) |
           ((uint64_t)in[4] << 24) |
           ((uint64_t)in[5] << 16) |
           ((uint64_t)in[6] << 8) |
           (uint64_t)in[7];
}

static uint32_t crc32_accumulate(uint32_t crc, const uint8_t *data, size_t length)
{
    size_t i;

    for (i = 0; i < length; i++) {
        int bit;
        crc ^= (uint32_t)data[i];
        for (bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }

    return crc;
}

static uint32_t crc32_message(const uint8_t header[PROTOCOL_HEADER_SIZE],
                              const uint8_t *payload,
                              uint32_t payload_size)
{
    uint32_t crc = 0xffffffffu;

    crc = crc32_accumulate(crc, header, PROTOCOL_HEADER_SIZE);
    if (payload_size > 0) {
        crc = crc32_accumulate(crc, payload, payload_size);
    }

    return crc ^ 0xffffffffu;
}

static uint32_t next_sequence(void)
{
    uint32_t sequence;

    pthread_mutex_lock(&tx_mutex);
    tx_sequence++;
    sequence = tx_sequence;
    pthread_mutex_unlock(&tx_mutex);

    return sequence;
}

static void make_transaction_id(uint64_t timestamp_ms,
                                uint64_t source_node,
                                uint8_t transaction_id[16])
{
    uint32_t source_low = (uint32_t)(source_node & 0xffffffffu);
    uint32_t sequence = next_sequence();

    write_u64(transaction_id, timestamp_ms);
    write_u32(transaction_id + 8, source_low);
    write_u32(transaction_id + 12, sequence);
}

static void encode_header(const protocol_header_t *header,
                          uint8_t out[PROTOCOL_HEADER_SIZE])
{
    write_u16(out + OFFSET_VERSION, header->version);
    write_u16(out + OFFSET_TYPE, header->type);
    write_u64(out + OFFSET_SOURCE, header->source_node);
    write_u64(out + OFFSET_DESTINATION, header->destination_node);
    memcpy(out + OFFSET_TRANSACTION, header->transaction_id, 16);
    write_u64(out + OFFSET_TIMESTAMP, header->timestamp_ms);
    write_u32(out + OFFSET_PAYLOAD_SIZE, header->payload_size);
    write_u32(out + OFFSET_CHECKSUM, header->checksum);
}

static void decode_header(const uint8_t in[PROTOCOL_HEADER_SIZE],
                          protocol_header_t *header)
{
    header->version = read_u16(in + OFFSET_VERSION);
    header->type = read_u16(in + OFFSET_TYPE);
    header->source_node = read_u64(in + OFFSET_SOURCE);
    header->destination_node = read_u64(in + OFFSET_DESTINATION);
    memcpy(header->transaction_id, in + OFFSET_TRANSACTION, 16);
    header->timestamp_ms = read_u64(in + OFFSET_TIMESTAMP);
    header->payload_size = read_u32(in + OFFSET_PAYLOAD_SIZE);
    header->checksum = read_u32(in + OFFSET_CHECKSUM);
}

uint64_t protocol_node_id_from_string(const char *text)
{
    const uint8_t *cursor = (const uint8_t *)text;
    uint64_t hash = 1469598103934665603ull;

    if (text == NULL || text[0] == '\0') {
        return 1u;
    }

    while (*cursor != '\0') {
        hash ^= (uint64_t)(*cursor);
        hash *= 1099511628211ull;
        cursor++;
    }

    return hash == 0u ? 1u : hash;
}

const char *protocol_message_type_name(uint16_t type)
{
    switch (type) {
    case MSG_JOIN:
        return "JOIN";
    case MSG_LEAVE:
        return "LEAVE";
    case MSG_LOOKUP:
        return "LOOKUP";
    case MSG_STORE:
        return "STORE";
    case MSG_DOWNLOAD_REQ:
        return "DOWNLOAD_REQ";
    case MSG_DOWNLOAD_REP:
        return "DOWNLOAD_REP";
    case MSG_PREPARE:
        return "PREPARE";
    case MSG_COMMIT:
        return "COMMIT";
    case MSG_ABORT:
        return "ABORT";
    case MSG_HEARTBEAT:
        return "HEARTBEAT";
    case MSG_GOSSIP:
        return "GOSSIP";
    case MSG_ELECTION:
        return "ELECTION";
    case MSG_OK:
        return "OK";
    case MSG_COORDINATOR:
        return "COORDINATOR";
    case MSG_SNAPSHOT:
        return "SNAPSHOT";
    case MSG_STATE_TRANSFER:
        return "STATE_TRANSFER";
    case MSG_ACK:
        return "ACK";
    case MSG_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

const char *protocol_error_string(int code)
{
    switch (code) {
    case PROTOCOL_OK:
        return "ok";
    case PROTOCOL_ERR_IO:
        return "I/O error";
    case PROTOCOL_ERR_CLOSED:
        return "connection closed";
    case PROTOCOL_ERR_VERSION:
        return "unsupported protocol version";
    case PROTOCOL_ERR_PAYLOAD_TOO_LARGE:
        return "payload too large";
    case PROTOCOL_ERR_CHECKSUM:
        return "checksum mismatch";
    case PROTOCOL_ERR_MEMORY:
        return "out of memory";
    case PROTOCOL_ERR_ARGUMENT:
        return "invalid argument";
    default:
        return "unknown protocol error";
    }
}

int protocol_send_message(int fd,
                          uint16_t type,
                          uint64_t source_node,
                          uint64_t destination_node,
                          const void *payload,
                          uint32_t payload_size)
{
    protocol_header_t header;
    uint8_t wire_header[PROTOCOL_HEADER_SIZE];
    const uint8_t *wire_payload = (const uint8_t *)payload;

    if (payload_size > PROTOCOL_MAX_PAYLOAD_SIZE) {
        return PROTOCOL_ERR_PAYLOAD_TOO_LARGE;
    }
    if (payload_size > 0 && payload == NULL) {
        return PROTOCOL_ERR_ARGUMENT;
    }

    memset(&header, 0, sizeof(header));
    header.version = PROTOCOL_VERSION;
    header.type = type;
    header.source_node = source_node;
    header.destination_node = destination_node;
    header.timestamp_ms = current_time_ms();
    header.payload_size = payload_size;
    make_transaction_id(header.timestamp_ms, source_node, header.transaction_id);

    header.checksum = 0;
    encode_header(&header, wire_header);
    header.checksum = crc32_message(wire_header, wire_payload, payload_size);
    encode_header(&header, wire_header);

    if (network_send_all(fd, wire_header, PROTOCOL_HEADER_SIZE) != (ssize_t)PROTOCOL_HEADER_SIZE) {
        return PROTOCOL_ERR_IO;
    }

    if (payload_size > 0 &&
        network_send_all(fd, wire_payload, payload_size) != (ssize_t)payload_size) {
        return PROTOCOL_ERR_IO;
    }

    return PROTOCOL_OK;
}

int protocol_recv_message(int fd, protocol_message_t *out)
{
    uint8_t wire_header[PROTOCOL_HEADER_SIZE];
    protocol_header_t header;
    uint8_t *payload = NULL;
    ssize_t received;
    uint32_t expected_checksum;

    if (out == NULL) {
        return PROTOCOL_ERR_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));

    received = network_recv_all(fd, wire_header, PROTOCOL_HEADER_SIZE);
    if (received == 0) {
        return PROTOCOL_ERR_CLOSED;
    }
    if (received != (ssize_t)PROTOCOL_HEADER_SIZE) {
        return PROTOCOL_ERR_IO;
    }

    decode_header(wire_header, &header);

    if (header.version != PROTOCOL_VERSION) {
        return PROTOCOL_ERR_VERSION;
    }

    if (header.payload_size > PROTOCOL_MAX_PAYLOAD_SIZE) {
        return PROTOCOL_ERR_PAYLOAD_TOO_LARGE;
    }

    if (header.payload_size > 0) {
        payload = malloc((size_t)header.payload_size + 1u);
        if (payload == NULL) {
            return PROTOCOL_ERR_MEMORY;
        }

        received = network_recv_all(fd, payload, header.payload_size);
        if (received != (ssize_t)header.payload_size) {
            free(payload);
            return received < 0 ? PROTOCOL_ERR_IO : PROTOCOL_ERR_CLOSED;
        }
        payload[header.payload_size] = '\0';
    }

    write_u32(wire_header + OFFSET_CHECKSUM, 0u);
    expected_checksum = crc32_message(wire_header, payload, header.payload_size);
    if (expected_checksum != header.checksum) {
        free(payload);
        return PROTOCOL_ERR_CHECKSUM;
    }

    out->header = header;
    out->payload = payload;
    return PROTOCOL_OK;
}

void protocol_free_message(protocol_message_t *message)
{
    if (message == NULL) {
        return;
    }

    free(message->payload);
    memset(message, 0, sizeof(*message));
}

void protocol_transaction_id_hex(const uint8_t transaction_id[16],
                                 char output[33])
{
    size_t i;

    for (i = 0; i < 16; i++) {
        snprintf(output + (i * 2u), 3u, "%02x", transaction_id[i]);
    }
    output[32] = '\0';
}

void protocol_dump_header(FILE *stream, const protocol_header_t *header)
{
    char transaction_hex[33];

    if (stream == NULL || header == NULL) {
        return;
    }

    protocol_transaction_id_hex(header->transaction_id, transaction_hex);
    fprintf(stream,
            "version=%u type=%s(%u) source=0x%016" PRIx64
            " destination=0x%016" PRIx64 " tx=%s timestamp_ms=%" PRIu64
            " payload_size=%" PRIu32 " checksum=0x%08" PRIx32 "\n",
            (unsigned int)header->version,
            protocol_message_type_name(header->type),
            (unsigned int)header->type,
            header->source_node,
            header->destination_node,
            transaction_hex,
            header->timestamp_ms,
            header->payload_size,
            header->checksum);
}
