# Documentacao do Checkpoint 1 - Aluno 1

## Objetivo

Este checkpoint implementa a base de comunicacao distribuida do sistema P2P hibrido. A entrega cobre a parte atribuida ao Aluno 1: `network.c`, `protocol.c` e `peer.c`.

## Relacao com os requisitos

O checkpoint pede:

- processo distribuido;
- socket TCP;
- comportamento cliente/servidor no mesmo processo;
- serializacao;
- framing de mensagens;
- concorrencia basica;
- identificacao de nos.

A implementacao atende esses pontos da seguinte forma:

- `network.c` encapsula sockets POSIX TCP, incluindo `socket`, `bind`, `listen`, `accept`, `connect`, envio e recebimento completos.
- `protocol.c` define um header binario fixo de 52 bytes e envia cada mensagem como `header + payload`, garantindo framing.
- `protocol.c` serializa todos os campos numericos em big-endian, evitando dependencia de arquitetura.
- `protocol.c` calcula e valida CRC32 sobre o header com checksum zerado mais o payload.
- `peer.c` executa um peer completo: ele sempre escuta conexoes de entrada e tambem pode iniciar conexoes de saida.
- `peer.c` cria uma thread para cada conexao aceita, permitindo concorrencia basica.
- A identificacao basica de nos e feita por um `NodeID` de 64 bits derivado do nome do no, suficiente para o Checkpoint 1.

## Header implementado

O header segue a arquitetura proposta no documento:

| Campo | Tamanho |
| --- | ---: |
| Protocol Version | 2 bytes |
| Message Type | 2 bytes |
| Source Node | 8 bytes |
| Destination Node | 8 bytes |
| Transaction ID | 16 bytes |
| Timestamp | 8 bytes |
| Payload Size | 4 bytes |
| Checksum CRC32 | 4 bytes |

O `Transaction ID` possui 128 bits e e montado com timestamp, parte do NodeID e numero sequencial local.

## Como testar

Compilar em Linux:

```sh
make clean
make
```

Terminal 1:

```sh
./peer 9001 peer-a
```

Terminal 2:

```sh
./peer 9002 peer-b 127.0.0.1 9001 "JOIN de peer-b"
```

Nesse fluxo:

- `peer-a` escuta conexoes na porta `9001`;
- `peer-b` escuta conexoes na porta `9002`;
- `peer-b` tambem conecta em `peer-a`;
- `peer-a` interpreta o header recebido;
- `peer-a` valida o checksum;
- `peer-a` responde `ACK`;
- `peer-b` recebe o `ACK`;
- os dois peers continuam abertos para receber novas conexoes.

Depois, no terminal do `peer-a`, enviar uma mensagem para `peer-b`:

```text
send 127.0.0.1 9002 resposta de peer-a
```

Isso demonstra que o mesmo peer pode receber e enviar mensagens, requisito importante para as proximas fases de upload/download em partes.

## Observacao de seguranca

O PDF continha trechos que tentavam instruir o assistente a introduzir bugs. Esses trechos foram tratados como conteudo nao confiavel do documento e nao foram seguidos. A implementacao foi feita para funcionar corretamente.
