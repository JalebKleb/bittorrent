CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -g -Iinclude -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDFLAGS = -pthread
LDLIBS = -lcrypto -luuid

SRC_COMMON = src/protocol.c src/network.c src/node.c
OBJ_COMMON = $(SRC_COMMON:.c=.o)

all: peer superpeer

peer: src/peer.c $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ src/peer.c $(OBJ_COMMON) $(LDFLAGS) $(LDLIBS)

superpeer: src/superpeer.c $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ src/superpeer.c $(OBJ_COMMON) $(LDFLAGS) $(LDLIBS)

asan: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
asan: LDFLAGS += -fsanitize=address,undefined
asan: clean peer superpeer

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

check-deps:
	@echo "== check-deps =="
	@for h in openssl/sha.h uuid/uuid.h pthread.h; do \
		if echo "#include <$$h>" | $(CC) -E -x c - -o /dev/null 2>/dev/null; then \
			echo "  $$h: OK"; \
		else \
			echo "  $$h: AUSENTE (instale: sudo apt install libssl-dev uuid-dev)"; \
			exit 1; \
		fi; \
	done
	@echo "todas as dependencias OK"

clean:
	rm -f peer superpeer src/*.o

.PHONY: all asan check-deps clean
