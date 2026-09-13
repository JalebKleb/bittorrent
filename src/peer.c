#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "network.h"
#include "node.h"
#include "protocol.h"

static int enviarPing(int fd, const uint8_t *meuId, const char *texto) {
    uint8_t destinoZero[NODE_ID_SIZE];
    Mensagem ped;
    int rc = -1;
    memset(&ped, 0, sizeof(ped));
    memset(destinoZero, 0, sizeof(destinoZero));
    if (iniciarMensagem(&ped, MSG_JOIN) != 0) {
        return -1;
    }
    definirPar(&ped, meuId, destinoZero);
    if (anexarCarga(&ped, (const uint8_t *)texto, (uint32_t)strlen(texto)) != 0) {
        liberarMensagem(&ped);
        return -1;
    }
    if (enviarMensagem(fd, &ped) == 0) {
        rc = 0;
    }
    liberarMensagem(&ped);
    return rc;
}

static int esperarAck(int fd) {
    Mensagem rep;
    int rc = 1;
    memset(&rep, 0, sizeof(rep));
    if (receberMensagem(fd, &rep) != 0) {
        fprintf(stderr, "peer: recv falhou\n");
        return 1;
    }
    printf("HEADER OK type=%s size=%u\n", tipoParaTexto(rep.cab.tipo), rep.cab.tamanho);
    printf("CRC OK\n");
    imprimirCabecalho(&rep.cab);
    if (rep.cab.tipo == MSG_ACK && rep.carga != NULL) {
        printf("ACK recebido: %.*s\n", rep.cab.tamanho, rep.carga);
        rc = 0;
    } else if (rep.cab.tipo == MSG_ACK) {
        printf("ACK recebido\n");
        rc = 0;
    } else {
        printf("resposta inesperada: %s\n", tipoParaTexto(rep.cab.tipo));
    }
    liberarMensagem(&rep);
    return rc;
}

static int executarCliente(const char *ip, int porta, const char *texto) {
    uint8_t meuId[NODE_ID_SIZE];
    int minhaPorta;
    int fd;
    int rc;
    minhaPorta = 40000 + (int)(getpid() % 20000);
    if (gerarNodeId("127.0.0.1", minhaPorta, meuId) != 0) {
        fprintf(stderr, "peer: NodeID falhou\n");
        return 1;
    }
    fixarPrefixoTxn(meuId);
    fd = conectarAoPeer(ip, porta);
    if (fd < 0) {
        perror("connect");
        return 1;
    }
    definirTimeout(fd, RECV_TIMEOUT_SEG);
    if (enviarPing(fd, meuId, texto) != 0) {
        fprintf(stderr, "peer: send falhou\n");
        fecharSocket(fd);
        return 1;
    }
    rc = esperarAck(fd);
    fecharSocket(fd);
    return rc;
}

int main(int argc, char **argv) {
    const char *ip = "127.0.0.1";
    const char *texto = "hello-cp1";
    uint8_t *gerada = NULL;
    int porta = 9001;
    int rc;
    if (argc >= 2) {
        ip = argv[1];
    }
    if (argc >= 3) {
        porta = atoi(argv[2]);
    }
    if (argc >= 4) {
        texto = argv[3];
    }
    if (porta <= 0 || porta > 65535) {
        fprintf(stderr, "uso: %s [ip] [porta] [texto|--big N]\n", argv[0]);
        return 1;
    }
    if (strcmp(texto, "--big") == 0) {
        long n = argc >= 5 ? atol(argv[4]) : 0;
        if (n <= 0 || n > (long)MAX_CARGA) {
            fprintf(stderr, "peer: N invalido (1..%u)\n", MAX_CARGA);
            return 1;
        }
        gerada = malloc((size_t)n + 1);
        if (gerada == NULL) {
            return 1;
        }
        memset(gerada, 'A', (size_t)n);
        gerada[n] = '\0';
        texto = (const char *)gerada;
    }
    rc = executarCliente(ip, porta, texto);
    free(gerada);
    return rc;
}
