# CP1 — Transporte + Protocolo + Identidade (18/09, 1.0pt, Aluno1/Aluno2)

## O que foi built
- Transporte TCP POSIX (socket/bind/listen/accept/connect) com `enviarTudo`/`receberTudo` em loop + timeout de recv (10s).
- Protocolo binário com header fixo de 100 bytes, serialização campo a campo (`hton*`/`ntoh*`) e CRC32 (header com soma zerada XOR CRC do payload).
- `txn_id` 128b thread-safe = `timestamp_ms(8) + NodeID[0..4](4) + seq(4)`.
- Identidade `NodeID = SHA256("IP|porta|uuid")` via OpenSSL + `uuid_generate` (libuuid).
- Tabela de membros em memória + mutex; superpeer com accept-loop + thread-per-conn (`pthread_detach`); peer em modo ping-pong JOIN→ACK.
- Tipos CP1 ativos: `JOIN, LEAVE, LOOKUP, HEARTBEAT, GOSSIP, ACK, ERROR`; demais retornam `ERROR not-implemented`.

## Arquivos
- `include/common.h`, `include/protocol.h`, `include/network.h`, `include/node.h`
- `src/network.c`, `src/protocol.c`, `src/peer.c`, `src/node.c`, `src/superpeer.c`
- `Makefile`, `tests/test_cp1.sh`

## Como rodar
```bash
make check-deps
make
./superpeer 9001 &
./peer 127.0.0.1 9001 "hello-cp1"
./peer 127.0.0.1 9001 --big 1048576   # carga de 1MB gerada internamente (framing)
tests/test_cp1.sh
make asan   # build com AddressSanitizer + UBSan
```

## Saída esperada
```
HEADER OK type=ACK size=8
CRC OK
ACK recebido: ACK:JOIN
```
No `superpeer` (stderr): `Header: ver=1 type=JOIN(1) size=... crc=... txn=...` e `superpeer: escutando porta 9001`.
Gate: `CP1 GATE: TODOS PASSARAM` (50/50 ciclos + 1MB + ASan).

## Formato do header (100 bytes no fio, big-endian)
| Campo | Tipo | Bytes | Offset | Notas |
|---|---|---|---|---|
| version | u16 | 2 | 0 | `PROTOCOL_VERSION = 1` |
| type | u16 | 2 | 2 | enum `TipoMensagem` |
| src | u8[32] | 32 | 4 | NodeID origem |
| dst | u8[32] | 32 | 36 | NodeID destino (zeros no JOIN) |
| txn_id | u8[16] | 16 | 68 | timestamp(8)+NodeID[0..4](4)+seq(4) |
| timestamp | u64 | 8 | 84 | ms desde epoch |
| payload_size | u32 | 4 | 92 | 0..8MB |
| checksum | u32 | 4 | 96 | CRC32(header zerado) XOR CRC32(payload) |
| payload | bytes | size | 100 | framing via `payload_size` + loop |

## Desvios do spec
Nenhum no protocolo. Decisões travadas aplicadas: UUID via libuuid (`-luuid`, sem fallback `/proc`); ASan como sanitizer padrão do gate (Valgrind opcional).

## Lições / próximo passo
- Framing parcial coberto por `receberTudo` (teste 1MB). Próximo: CP2 (chunk 4MB, SHA-256 por chunk, LZ4, metadata + `FileMetadata`).
