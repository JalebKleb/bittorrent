CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -O2 -D_POSIX_C_SOURCE=200809L -pthread
LDFLAGS ?=
LDLIBS ?=

TARGET = peer
OBJS = peer.o network.o protocol.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS)
