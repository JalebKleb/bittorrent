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

static uint8_t gMeuId[NODE_ID_SIZE];
static int gTemId = 0;

static int gerarIdentidade(void) {
    int minhaPorta;
    minhaPorta = 40000 + (int)(getpid() % 20000);
    if (gerarNodeId("127.0.0.1", minhaPorta, gMeuId) != 0) {
        fprintf(stderr, "peer: NodeID falhou\n");
        return -1;
    }
    fixarPrefixoTxn(gMeuId);
    gTemId = 1;
    return 0;
}

static int conversarComPeer(const char *ip, int porta, const char *texto) {
    int fd;
    int rc;
    if (ip == NULL || texto == NULL) {
        return -1;
    }
    if (porta <= 0 || porta > 65535) {
        return -1;
    }
    if (strlen(texto) > MAX_CARGA) {
        fprintf(stderr, "peer: mensagem excede %u bytes\n", MAX_CARGA);
        return -1;
    }
    if (!gTemId) {
        if (gerarIdentidade() != 0) {
            return -1;
        }
    }
    fd = conectarAoPeer(ip, porta);
    if (fd < 0) {
        perror("connect");
        return -1;
    }
    definirTimeout(fd, RECV_TIMEOUT_SEG);
    if (enviarPing(fd, gMeuId, texto) != 0) {
        fprintf(stderr, "peer: send falhou\n");
        fecharSocket(fd);
        return -1;
    }
    rc = esperarAck(fd);
    fecharSocket(fd);
    if (rc != 0) {
        return -1;
    }
    return 0;
}

static void removerNovaLinha(char *texto) {
    size_t n;
    if (texto == NULL) {
        return;
    }
    n = strlen(texto);
    while (n > 0 && (texto[n - 1] == '\n' || texto[n - 1] == '\r')) {
        texto[n - 1] = '\0';
        n--;
    }
}

static void mostrarAjudaDebug(void) {
    printf("comandos:\n");
    printf("  send <host> <porta> <mensagem>\n");
    printf("  quit\n");
}

static int processarLinha(char *linha) {
    char host[256];
    char portaTxt[32];
    char *msg;
    int porta;
    int lidos;
    if (linha == NULL) {
        return 0;
    }
    if (linha[0] == '\0') {
        return 0;
    }
    if (strcmp(linha, "quit") == 0 || strcmp(linha, "exit") == 0) {
        return 1;
    }
    if (strncmp(linha, "send ", 5) != 0) {
        fprintf(stderr, "peer: comando desconhecido (use send|quit)\n");
        return 0;
    }
    msg = linha + 5;
    while (*msg == ' ' || *msg == '\t') {
        msg++;
    }
    if (sscanf(msg, "%255s %31s %n", host, portaTxt, &lidos) < 2) {
        fprintf(stderr, "peer: uso: send <host> <porta> <mensagem>\n");
        return 0;
    }
    porta = atoi(portaTxt);
    if (porta <= 0 || porta > 65535) {
        fprintf(stderr, "peer: porta invalida\n");
        return 0;
    }
    msg = msg + lidos;
    while (*msg == ' ' || *msg == '\t') {
        msg++;
    }
    if (*msg == '\0') {
        fprintf(stderr, "peer: uso: send <host> <porta> <mensagem>\n");
        return 0;
    }
    if (conversarComPeer(host, porta, msg) != 0) {
        fprintf(stderr, "peer: send falhou\n");
    }
    return 0;
}

static int executarModoDebug(void) {
    char linha[2048];
    if (gerarIdentidade() != 0) {
        return 1;
    }
    printf("peer debug: NodeID pronto, digite send|quit\n");
    mostrarAjudaDebug();
    while (fgets(linha, sizeof(linha), stdin) != NULL) {
        int rc;
        removerNovaLinha(linha);
        rc = processarLinha(linha);
        if (rc == 1) {
            break;
        }
    }
    printf("peer: encerrado\n");
    return 0;
}

static int executarCliente(const char *ip, int porta, const char *texto) {
    if (gerarIdentidade() != 0) {
        return 1;
    }
    if (conversarComPeer(ip, porta, texto) != 0) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *ip = "127.0.0.1";
    const char *texto = "hello-cp1";
    uint8_t *gerada = NULL;
    int porta = 9001;
    int rc;
    if (argc >= 2 && strcmp(argv[1], "--debug") == 0) {
        if (argc != 2 && argc != 4) {
            fprintf(stderr, "uso: %s --debug [ip] [porta]\n", argv[0]);
            return 1;
        }
        if (argc == 4) {
            porta = atoi(argv[3]);
            if (argv[2][0] == '\0' || porta <= 0 || porta > 65535) {
                fprintf(stderr, "uso: %s --debug [ip] [porta]\n", argv[0]);
                return 1;
            }
            printf("peer debug: alvo sugerido %s %d\n", argv[2], porta);
        }
        return executarModoDebug();
    }
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
        fprintf(stderr, "     %s --debug [ip] [porta]\n", argv[0]);
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
