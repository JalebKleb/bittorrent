#include <openssl/sha.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <uuid/uuid.h>

#include "node.h"
#include "protocol.h"

static Membro gTabela[MAX_MEMBROS];
static int gTotal = 0;
static pthread_mutex_t gTravaMembros = PTHREAD_MUTEX_INITIALIZER;

int gerarNodeId(const char *ip, int porta, uint8_t saida[NODE_ID_SIZE]) {
    uuid_t uid;
    char texto[37];
    char entrada[128];
    int n;
    if (ip == NULL || saida == NULL) {
        return -1;
    }
    if (porta <= 0 || porta > 65535) {
        return -1;
    }
    uuid_generate(uid);
    uuid_unparse_lower(uid, texto);
    n = snprintf(entrada, sizeof(entrada), "%s|%d|%s", ip, porta, texto);
    if (n <= 0 || n >= (int)sizeof(entrada)) {
        return -1;
    }
    SHA256((unsigned char *)entrada, (size_t)n, saida);
    return 0;
}

int compararNodeId(const uint8_t *a, const uint8_t *b) {
    if (a == NULL || b == NULL) {
        return -1;
    }
    return memcmp(a, b, NODE_ID_SIZE);
}

void imprimirNodeId(const uint8_t *id) {
    int i;
    if (id == NULL) {
        return;
    }
    for (i = 0; i < NODE_ID_SIZE; i++) {
        fprintf(stderr, "%02x", id[i]);
    }
}

int adicionarMembro(const uint8_t *id, const char *ip, int porta) {
    int i;
    if (id == NULL || ip == NULL || porta < 0) {
        return -1;
    }
    pthread_mutex_lock(&gTravaMembros);
    for (i = 0; i < gTotal; i++) {
        if (memcmp(gTabela[i].id, id, NODE_ID_SIZE) == 0) {
            gTabela[i].ultimoContato = agoraMs();
            pthread_mutex_unlock(&gTravaMembros);
            return 0;
        }
    }
    if (gTotal >= MAX_MEMBROS) {
        pthread_mutex_unlock(&gTravaMembros);
        return -1;
    }
    memcpy(gTabela[gTotal].id, id, NODE_ID_SIZE);
    snprintf(gTabela[gTotal].ip, sizeof(gTabela[gTotal].ip), "%s", ip);
    gTabela[gTotal].porta = porta;
    gTabela[gTotal].estado = EST_ATIVO;
    gTabela[gTotal].ultimoContato = agoraMs();
    gTabela[gTotal].versao = 1;
    gTotal++;
    pthread_mutex_unlock(&gTravaMembros);
    return 0;
}

int buscarMembro(const uint8_t *id, Membro *saida) {
    int i;
    if (id == NULL || saida == NULL) {
        return -1;
    }
    pthread_mutex_lock(&gTravaMembros);
    for (i = 0; i < gTotal; i++) {
        if (memcmp(gTabela[i].id, id, NODE_ID_SIZE) == 0) {
            *saida = gTabela[i];
            pthread_mutex_unlock(&gTravaMembros);
            return 0;
        }
    }
    pthread_mutex_unlock(&gTravaMembros);
    return -1;
}

int atualizarMembro(const uint8_t *id, int estado) {
    int i;
    if (id == NULL) {
        return -1;
    }
    pthread_mutex_lock(&gTravaMembros);
    for (i = 0; i < gTotal; i++) {
        if (memcmp(gTabela[i].id, id, NODE_ID_SIZE) == 0) {
            gTabela[i].estado = estado;
            gTabela[i].ultimoContato = agoraMs();
            gTabela[i].versao++;
            pthread_mutex_unlock(&gTravaMembros);
            return 0;
        }
    }
    pthread_mutex_unlock(&gTravaMembros);
    return -1;
}

int removerMembro(const uint8_t *id) {
    int i;
    int j;
    if (id == NULL) {
        return -1;
    }
    pthread_mutex_lock(&gTravaMembros);
    for (i = 0; i < gTotal; i++) {
        if (memcmp(gTabela[i].id, id, NODE_ID_SIZE) == 0) {
            for (j = i; j + 1 < gTotal; j++) {
                gTabela[j] = gTabela[j + 1];
            }
            gTotal--;
            pthread_mutex_unlock(&gTravaMembros);
            return 0;
        }
    }
    pthread_mutex_unlock(&gTravaMembros);
    return -1;
}

int contarMembros(void) {
    int n;
    pthread_mutex_lock(&gTravaMembros);
    n = gTotal;
    pthread_mutex_unlock(&gTravaMembros);
    return n;
}
