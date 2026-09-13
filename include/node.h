#ifndef NODE_H
#define NODE_H

#include <stdint.h>
#include "common.h"

typedef struct {
    uint8_t id[NODE_ID_SIZE];
    char ip[64];
    int porta;
    int estado;
    uint64_t ultimoContato;
    uint64_t versao;
} Membro;

int gerarNodeId(const char *ip, int porta, uint8_t saida[NODE_ID_SIZE]);
int adicionarMembro(const uint8_t *id, const char *ip, int porta);
int buscarMembro(const uint8_t *id, Membro *saida);
int atualizarMembro(const uint8_t *id, int estado);
int removerMembro(const uint8_t *id);
int contarMembros(void);
void imprimirNodeId(const uint8_t *id);
int compararNodeId(const uint8_t *a, const uint8_t *b);

#endif
