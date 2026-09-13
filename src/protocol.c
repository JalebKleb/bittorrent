#include <arpa/inet.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "protocol.h"

static uint32_t gTabelaCrc[256];
static int gTabelaPronta = 0;
static pthread_mutex_t gTravaTubo = PTHREAD_MUTEX_INITIALIZER;
static uint8_t gPrefixo[4] = {0, 0, 0, 0};
static uint32_t gSeq = 0;

static void montarTabelaCrc(void) {
    uint32_t i;
    int j;
    if (gTabelaPronta) {
        return;
    }
    for (i = 0; i < 256; i++) {
        uint32_t v = i;
        for (j = 0; j < 8; j++) {
            if (v & 1u) {
                v = (v >> 1) ^ 0xEDB88320u;
            } else {
                v >>= 1;
            }
        }
        gTabelaCrc[i] = v;
    }
    gTabelaPronta = 1;
}

uint32_t calcularChecksum(const uint8_t *buf, size_t len) {
    uint32_t soma = 0xFFFFFFFFu;
    size_t i;
    if (buf == NULL) {
        return 0;
    }
    montarTabelaCrc();
    for (i = 0; i < len; i++) {
        soma = gTabelaCrc[(soma ^ buf[i]) & 0xFFu] ^ (soma >> 8);
    }
    return soma ^ 0xFFFFFFFFu;
}

uint64_t agoraMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

void fixarPrefixoTxn(const uint8_t *nodeId) {
    if (nodeId == NULL) {
        return;
    }
    pthread_mutex_lock(&gTravaTubo);
    memcpy(gPrefixo, nodeId, 4);
    pthread_mutex_unlock(&gTravaTubo);
}

void gerarTxnId(uint8_t saida[TXN_ID_SIZE]) {
    uint64_t tempo;
    uint32_t seq;
    int i;
    if (saida == NULL) {
        return;
    }
    pthread_mutex_lock(&gTravaTubo);
    tempo = agoraMs();
    seq = gSeq++;
    for (i = 0; i < 8; i++) {
        saida[i] = (uint8_t)(tempo >> (56 - i * 8));
    }
    memcpy(saida + 8, gPrefixo, 4);
    for (i = 0; i < 4; i++) {
        saida[12 + i] = (uint8_t)(seq >> (24 - i * 8));
    }
    pthread_mutex_unlock(&gTravaTubo);
}

static void gravarU16(uint8_t *buf, uint16_t v) {
    uint16_t n = htons(v);
    memcpy(buf, &n, 2);
}

static void gravarU32(uint8_t *buf, uint32_t v) {
    uint32_t n = htonl(v);
    memcpy(buf, &n, 4);
}

static void gravarU64(uint8_t *buf, uint64_t v) {
    int i;
    for (i = 0; i < 8; i++) {
        buf[i] = (uint8_t)(v >> (56 - i * 8));
    }
}

static uint16_t lerU16(const uint8_t *buf) {
    uint16_t n;
    memcpy(&n, buf, 2);
    return ntohs(n);
}

static uint32_t lerU32(const uint8_t *buf) {
    uint32_t n;
    memcpy(&n, buf, 4);
    return ntohl(n);
}

static uint64_t lerU64(const uint8_t *buf) {
    uint64_t v = 0;
    int i;
    for (i = 0; i < 8; i++) {
        v = (v << 8) | buf[i];
    }
    return v;
}

int serializarCabecalho(const Cabecalho *cab, uint8_t saida[HEADER_WIRE_SIZE]) {
    if (cab == NULL || saida == NULL) {
        return -1;
    }
    gravarU16(saida + 0, cab->versao);
    gravarU16(saida + 2, cab->tipo);
    memcpy(saida + 4, cab->origem, NODE_ID_SIZE);
    memcpy(saida + 36, cab->destino, NODE_ID_SIZE);
    memcpy(saida + 68, cab->txnId, TXN_ID_SIZE);
    gravarU64(saida + 84, cab->carimbo);
    gravarU32(saida + 92, cab->tamanho);
    gravarU32(saida + 96, cab->soma);
    return 0;
}

int deserializarCabecalho(const uint8_t *buf, Cabecalho *cab) {
    if (buf == NULL || cab == NULL) {
        return -1;
    }
    cab->versao = lerU16(buf + 0);
    cab->tipo = lerU16(buf + 2);
    memcpy(cab->origem, buf + 4, NODE_ID_SIZE);
    memcpy(cab->destino, buf + 36, NODE_ID_SIZE);
    memcpy(cab->txnId, buf + 68, TXN_ID_SIZE);
    cab->carimbo = lerU64(buf + 84);
    cab->tamanho = lerU32(buf + 92);
    cab->soma = lerU32(buf + 96);
    if (cab->versao != PROTOCOL_VERSION) {
        return -1;
    }
    if (cab->tamanho > MAX_CARGA) {
        return -1;
    }
    return 0;
}

void imprimirCabecalho(const Cabecalho *cab) {
    int i;
    if (cab == NULL) {
        return;
    }
    fprintf(stderr, "Header: ver=%u type=%s(%u) size=%u crc=%08x txn=",
        cab->versao, tipoParaTexto(cab->tipo), cab->tipo,
        cab->tamanho, cab->soma);
    for (i = 0; i < TXN_ID_SIZE; i++) {
        fprintf(stderr, "%02x", cab->txnId[i]);
    }
    fprintf(stderr, "\n");
}

int iniciarMensagem(Mensagem *msg, uint16_t tipo) {
    if (msg == NULL) {
        return -1;
    }
    memset(msg, 0, sizeof(*msg));
    msg->cab.versao = PROTOCOL_VERSION;
    msg->cab.tipo = tipo;
    msg->cab.carimbo = agoraMs();
    gerarTxnId(msg->cab.txnId);
    return 0;
}

int definirPar(Mensagem *msg, const uint8_t *origem, const uint8_t *destino) {
    if (msg == NULL || origem == NULL || destino == NULL) {
        return -1;
    }
    memcpy(msg->cab.origem, origem, NODE_ID_SIZE);
    memcpy(msg->cab.destino, destino, NODE_ID_SIZE);
    return 0;
}

int anexarCarga(Mensagem *msg, const uint8_t *buf, uint32_t len) {
    if (msg == NULL) {
        return -1;
    }
    if (len > MAX_CARGA) {
        return -1;
    }
    if (len == 0) {
        msg->carga = NULL;
        msg->cab.tamanho = 0;
        return 0;
    }
    if (buf == NULL) {
        return -1;
    }
    msg->carga = malloc(len);
    if (msg->carga == NULL) {
        return -1;
    }
    memcpy(msg->carga, buf, len);
    msg->cab.tamanho = len;
    return 0;
}

void liberarMensagem(Mensagem *msg) {
    if (msg == NULL) {
        return;
    }
    free(msg->carga);
    msg->carga = NULL;
    msg->cab.tamanho = 0;
}

const char *tipoParaTexto(uint16_t tipo) {
    switch (tipo) {
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
