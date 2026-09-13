#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define PROTOCOL_VERSION 1
#define NODE_ID_SIZE 32
#define TXN_ID_SIZE 16
#define HEADER_WIRE_SIZE 100
#define MAX_CARGA (8u * 1024u * 1024u)
#define MAX_MEMBROS 64
#define RECV_TIMEOUT_SEG 10

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
} TipoMensagem;

typedef enum {
    EST_DESCONECTADO = 0,
    EST_CONECTANDO = 1,
    EST_CONECTADO = 2,
    EST_AUTENTICADO = 3,
    EST_SINCRONIZADO = 4,
    EST_ATIVO = 5,
    EST_SAINDO = 6
} EstadoConexao;

typedef struct {
    uint16_t versao;
    uint16_t tipo;
    uint8_t origem[NODE_ID_SIZE];
    uint8_t destino[NODE_ID_SIZE];
    uint8_t txnId[TXN_ID_SIZE];
    uint64_t carimbo;
    uint32_t tamanho;
    uint32_t soma;
} Cabecalho;

typedef struct {
    Cabecalho cab;
    uint8_t *carga;
} Mensagem;

#endif
