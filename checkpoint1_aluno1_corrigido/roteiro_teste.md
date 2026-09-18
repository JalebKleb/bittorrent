# Roteiro de teste - Checkpoint 1

## 1. Compilar

```sh
make clean
make
```

Resultado esperado: binario `peer` gerado sem erros.

## 2. Rodar o primeiro peer

Terminal 1:

```sh
./peer 9001 peer-a
```

Resultado esperado:

```text
[node] 'peer-a' node_id=0x... escutando na porta 9001
[node] comandos disponiveis:
       send <host> <porta> <mensagem>
       quit
```

## 3. Rodar o segundo peer ja conectando no primeiro

Terminal 2:

```sh
./peer 9002 peer-b 127.0.0.1 9001 "JOIN de peer-b"
```

Resultado esperado no `peer-b`:

```text
[node] 'peer-b' node_id=0x... escutando na porta 9002
[outbound] conectado em 127.0.0.1:9001
[outbound] mensagem enviada com framing e checksum
[outbound] header recebido: version=1 type=ACK(17) ...
payload (... bytes): ACK: mensagem recebida e checksum validado
```

Resultado esperado no `peer-a`:

```text
[inbound] conexao aceita de ...
[inbound] header recebido: version=1 type=JOIN(1) ...
payload (... bytes): JOIN de peer-b
```

## 4. Enviar uma mensagem de volta

No terminal do `peer-a`:

```text
send 127.0.0.1 9002 resposta de peer-a
```

Resultado esperado:

- `peer-a` inicia uma conexao de saida para `peer-b`;
- `peer-b` recebe a mensagem;
- `peer-b` interpreta o header;
- `peer-b` valida o checksum;
- `peer-a` recebe o `ACK`.

## 5. Testar concorrencia

Com `peer-a` aberto, iniciar mais peers apontando para ele:

```sh
./peer 9003 peer-c 127.0.0.1 9001 "mensagem de peer-c"
./peer 9004 peer-d 127.0.0.1 9001 "mensagem de peer-d"
```

Resultado esperado: `peer-a` aceita varias conexoes e cria uma thread para cada uma.

Esse teste demonstra o comportamento esperado em um sistema tipo BitTorrent: cada peer pode receber partes de outros peers e tambem enviar partes para outros peers no mesmo processo.
