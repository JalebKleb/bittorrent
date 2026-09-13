#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>
#include <stdint.h>
#include "common.h"

int escutarPorta(int porta);
int conectarAoPeer(const char *ip, int porta);
int definirTimeout(int fd, int segundos);
int enviarTudo(int fd, const uint8_t *buf, size_t len);
int receberTudo(int fd, uint8_t *buf, size_t len);
int enviarMensagem(int fd, const Mensagem *msg);
int receberMensagem(int fd, Mensagem *msg);
void fecharSocket(int fd);

#endif
