#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include "common.h"

uint32_t calcularChecksum(const uint8_t *buf, size_t len);
void fixarPrefixoTxn(const uint8_t *nodeId);
void gerarTxnId(uint8_t saida[TXN_ID_SIZE]);
uint64_t agoraMs(void);
int serializarCabecalho(const Cabecalho *cab, uint8_t saida[HEADER_WIRE_SIZE]);
int deserializarCabecalho(const uint8_t *buf, Cabecalho *cab);
void imprimirCabecalho(const Cabecalho *cab);
int iniciarMensagem(Mensagem *msg, uint16_t tipo);
int definirPar(Mensagem *msg, const uint8_t *origem, const uint8_t *destino);
int anexarCarga(Mensagem *msg, const uint8_t *buf, uint32_t len);
void liberarMensagem(Mensagem *msg);
const char *tipoParaTexto(uint16_t tipo);

#endif
