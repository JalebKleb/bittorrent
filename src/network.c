#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "network.h"
#include "protocol.h"

int escutarPorta(int porta) {
    int fd;
    int um = 1;
    struct sockaddr_in addr;
    if (porta <= 0 || porta > 65535) {
        return -1;
    }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &um, sizeof(um)) < 0) {
        fecharSocket(fd);
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)porta);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fecharSocket(fd);
        return -1;
    }
    if (listen(fd, 16) < 0) {
        fecharSocket(fd);
        return -1;
    }
    return fd;
}

int conectarAoPeer(const char *ip, int porta) {
    int fd;
    struct sockaddr_in addr;
    if (ip == NULL || porta <= 0 || porta > 65535) {
        return -1;
    }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)porta);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fecharSocket(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fecharSocket(fd);
        return -1;
    }
    return fd;
}

int definirTimeout(int fd, int segundos) {
    struct timeval tv;
    if (fd < 0 || segundos <= 0) {
        return -1;
    }
    tv.tv_sec = segundos;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return -1;
    }
    return 0;
}

int enviarTudo(int fd, const uint8_t *buf, size_t len) {
    size_t enviado = 0;
    if (fd < 0 || (buf == NULL && len > 0)) {
        return -1;
    }
    while (enviado < len) {
        ssize_t n = send(fd, buf + enviado, len - enviado, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        enviado += (size_t)n;
    }
    return 0;
}

int receberTudo(int fd, uint8_t *buf, size_t len) {
    size_t lido = 0;
    if (fd < 0 || (buf == NULL && len > 0)) {
        return -1;
    }
    while (lido < len) {
        ssize_t n = recv(fd, buf + lido, len - lido, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        lido += (size_t)n;
    }
    return 0;
}

static uint32_t somarMensagem(const Cabecalho *cab, const uint8_t *carga) {
    uint8_t fio[HEADER_WIRE_SIZE];
    Cabecalho copia;
    uint32_t soma;
    if (cab == NULL) {
        return 0;
    }
    copia = *cab;
    copia.soma = 0;
    serializarCabecalho(&copia, fio);
    soma = calcularChecksum(fio, sizeof(fio));
    if (carga != NULL && cab->tamanho > 0) {
        uint32_t s2 = calcularChecksum(carga, cab->tamanho);
        soma ^= s2;
    }
    return soma;
}

int enviarMensagem(int fd, const Mensagem *msg) {
    uint8_t fio[HEADER_WIRE_SIZE];
    Cabecalho copia;
    if (fd < 0 || msg == NULL) {
        return -1;
    }
    copia = msg->cab;
    copia.soma = somarMensagem(&msg->cab, msg->carga);
    if (serializarCabecalho(&copia, fio) != 0) {
        return -1;
    }
    if (enviarTudo(fd, fio, sizeof(fio)) != 0) {
        return -1;
    }
    if (msg->cab.tamanho > 0) {
        if (msg->carga == NULL) {
            return -1;
        }
        if (enviarTudo(fd, msg->carga, msg->cab.tamanho) != 0) {
            return -1;
        }
    }
    return 0;
}

int receberMensagem(int fd, Mensagem *msg) {
    uint8_t fio[HEADER_WIRE_SIZE];
    uint32_t esperada;
    if (fd < 0 || msg == NULL) {
        return -1;
    }
    memset(msg, 0, sizeof(*msg));
    if (receberTudo(fd, fio, sizeof(fio)) != 0) {
        return -1;
    }
    if (deserializarCabecalho(fio, &msg->cab) != 0) {
        return -1;
    }
    if (msg->cab.tamanho > 0) {
        msg->carga = malloc(msg->cab.tamanho);
        if (msg->carga == NULL) {
            return -1;
        }
        if (receberTudo(fd, msg->carga, msg->cab.tamanho) != 0) {
            liberarMensagem(msg);
            return -1;
        }
    }
    esperada = somarMensagem(&msg->cab, msg->carga);
    if (esperada != msg->cab.soma) {
        liberarMensagem(msg);
        errno = EBADMSG;
        return -1;
    }
    return 0;
}

void fecharSocket(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}
