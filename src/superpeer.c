#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "network.h"
#include "node.h"
#include "protocol.h"

static uint8_t gId[NODE_ID_SIZE];

static int montarResposta(Mensagem *resp, const Mensagem *pedido, uint16_t tipo) {
    if (resp == NULL || pedido == NULL) {
        return -1;
    }
    if (iniciarMensagem(resp, tipo) != 0) {
        return -1;
    }
    if (definirPar(resp, gId, pedido->cab.origem) != 0) {
        return -1;
    }
    memcpy(resp->cab.txnId, pedido->cab.txnId, TXN_ID_SIZE);
    return 0;
}

static int responderTexto(int fd, const Mensagem *pedido, uint16_t tipo, const char *texto) {
    Mensagem resp;
    int rc;
    memset(&resp, 0, sizeof(resp));
    if (montarResposta(&resp, pedido, tipo) != 0) {
        return -1;
    }
    if (anexarCarga(&resp, (const uint8_t *)texto, (uint32_t)strlen(texto)) != 0) {
        liberarMensagem(&resp);
        return -1;
    }
    rc = enviarMensagem(fd, &resp);
    liberarMensagem(&resp);
    return rc;
}

static int processarPedido(int fd, const Mensagem *pedido) {
    char eco[256];
    switch (pedido->cab.tipo) {
    case MSG_JOIN:
        adicionarMembro(pedido->cab.origem, "127.0.0.1", 0);
        return responderTexto(fd, pedido, MSG_ACK, "ACK:JOIN");
    case MSG_LEAVE:
        atualizarMembro(pedido->cab.origem, EST_SAINDO);
        removerMembro(pedido->cab.origem);
        return responderTexto(fd, pedido, MSG_ACK, "ACK:LEAVE");
    case MSG_LOOKUP:
    case MSG_HEARTBEAT:
    case MSG_GOSSIP:
        snprintf(eco, sizeof(eco), "ACK:%s", tipoParaTexto(pedido->cab.tipo));
        return responderTexto(fd, pedido, MSG_ACK, eco);
    case MSG_ACK:
        fprintf(stderr, "superpeer: ACK recebido (sem resposta)\n");
        return 0;
    default:
        return responderTexto(fd, pedido, MSG_ERROR, "ERROR not-implemented");
    }
}

static void *tratarConexao(void *arg) {
    int fd;
    Mensagem pedido;
    if (arg == NULL) {
        return NULL;
    }
    fd = *(int *)arg;
    free(arg);
    definirTimeout(fd, RECV_TIMEOUT_SEG);
    memset(&pedido, 0, sizeof(pedido));
    if (receberMensagem(fd, &pedido) != 0) {
        fprintf(stderr, "superpeer: recv falhou\n");
        fecharSocket(fd);
        return NULL;
    }
    imprimirCabecalho(&pedido.cab);
    processarPedido(fd, &pedido);
    liberarMensagem(&pedido);
    fecharSocket(fd);
    return NULL;
}

static int executarSuperPeer(int porta) {
    int escuta;
    if (gerarNodeId("127.0.0.1", porta, gId) != 0) {
        fprintf(stderr, "superpeer: NodeID falhou\n");
        return -1;
    }
    fixarPrefixoTxn(gId);
    fprintf(stderr, "superpeer: NodeID=");
    imprimirNodeId(gId);
    fprintf(stderr, " porta=%d\n", porta);
    escuta = escutarPorta(porta);
    if (escuta < 0) {
        perror("listen");
        return -1;
    }
    fprintf(stderr, "superpeer: escutando porta %d\n", porta);
    for (;;) {
        int *pfd = malloc(sizeof(int));
        pthread_t tid;
        if (pfd == NULL) {
            continue;
        }
        *pfd = accept(escuta, NULL, NULL);
        if (*pfd < 0) {
            free(pfd);
            continue;
        }
        if (pthread_create(&tid, NULL, tratarConexao, pfd) != 0) {
            fecharSocket(*pfd);
            free(pfd);
            continue;
        }
        pthread_detach(tid);
    }
}

int main(int argc, char **argv) {
    int porta = 9001;
    if (argc >= 2) {
        porta = atoi(argv[1]);
    }
    if (porta <= 0 || porta > 65535) {
        fprintf(stderr, "uso: %s [porta]\n", argv[0]);
        return 1;
    }
    if (executarSuperPeer(porta) != 0) {
        return 1;
    }
    return 0;
}
