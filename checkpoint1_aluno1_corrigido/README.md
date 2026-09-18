# Checkpoint 1 - Aluno 1

Implementacao em C/POSIX para o primeiro checkpoint do trabalho de Programacao Distribuida.

O escopo coberto aqui e o do Aluno 1:

- `network.c`: criacao de socket TCP, `bind`, `listen`, `accept`, `connect`, `send` e `recv`.
- `protocol.c`: serializacao, framing, header binario, TransactionID de 128 bits e CRC32.
- `peer.c`: processo P2P que sempre escuta conexoes e tambem pode iniciar conexoes de saida, com concorrencia basica usando POSIX Threads e identificacao de nos.

## Compilacao

Em Linux:

```sh
make
```

## Execucao

Cada execucao do binario representa um peer completo. Ele nao possui modo separado de cliente ou servidor: todo peer abre uma porta TCP para receber conexoes e, ao mesmo tempo, pode conectar em outros peers para enviar mensagens.

Em maquinas diferentes, use o IP IPv4 da maquina de destino no comando `send` ou na conexao inicial. O listener abre a porta em IPv4 para evitar que uma maquina fique escutando apenas em IPv6 e outra tente acessar por IPv4.

Terminal 1:

```sh
./peer 9001 peer-a
```

Terminal 2:

```sh
./peer 9002 peer-b 127.0.0.1 9001 "JOIN de peer-b para peer-a"
```

Enquanto os dois processos estiverem abertos, cada um aceita conexoes de entrada. No prompt de qualquer peer, tambem e possivel enviar mensagens para outro:

```text
send 127.0.0.1 9002 mensagem enviada de peer-a para peer-b
```

Para encerrar:

```text
quit
```

## Diagnostico rapido de envio/recebimento

Se uma maquina nao recebe mensagens:

- confirme que o processo `peer` continua aberto na maquina destino;
- confirme que a porta usada no comando e a mesma porta exibida pelo peer destino;
- use o IP IPv4 da maquina destino, nao o nome da maquina, se houver duvida de DNS;
- libere a porta no firewall do sistema operacional;
- nao use a mesma porta para dois peers na mesma maquina.

Se uma maquina nao envia mensagens:

- confirme que o peer destino ja iniciou e exibiu que esta escutando;
- confira se o host e a porta no comando `send <host> <porta> <mensagem>` apontam para o peer destino;
- verifique se o peer destino responde com `ACK`, pois o emissor espera essa resposta para confirmar framing e checksum.

## Formato do header

O header possui 52 bytes em big-endian:

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

O checksum e calculado sobre o header com o campo `Checksum` zerado mais o payload.

## Itens de verificacao do checkpoint

- Conexao TCP funcionando: um peer conecta em outro peer.
- Mensagem chega corretamente: o peer receptor imprime o payload recebido.
- Header e interpretado: `protocol_dump_header` mostra tipo, origem, destino, transacao, timestamp, tamanho e checksum.
- Checksum e validado: `protocol_recv_message` rejeita mensagens corrompidas.
- Dois processos conversam: o peer emissor recebe `ACK` do peer receptor.
- Peer P2P simultaneo: todo processo `peer` escuta conexoes enquanto permite envio para outros peers.
- Concorrencia basica: cada conexao aceita roda em uma thread destacada.
- Sem segmentation fault esperado: entradas e tamanhos sao validados antes de alocar/ler payload.
