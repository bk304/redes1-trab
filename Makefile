.PHONY: all debug lo clean purge

all: client server

WARNING = -Wall -Wextra -Wno-packed-bitfield-compat

CFLAGS =

# Alvo "debug"
ifeq ($(filter debug,$(MAKECMDGOALS)),debug)
CFLAGS += -DDEBUG
endif

RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS)) \
	$(eval $(RUN_ARGS):;@:)

client: Client.c ConexaoRawSocket.o message.o pilha.o tokenlizer.o
	gcc $(CFLAGS) -o client Client.c ConexaoRawSocket.o message.o pilha.o tokenlizer.o $(WARNING)

server: Server.c ConexaoRawSocket.o message.o pilha.o
	gcc $(CFLAGS) -o server Server.c ConexaoRawSocket.o message.o pilha.o $(WARNING)

ConexaoRawSocket.o: ConexaoRawSocket.c ConexaoRawSocket.h
	gcc $(CFLAGS) -o ConexaoRawSocket.o -c ConexaoRawSocket.c $(WARNING)

message.o: message.c message.h
	gcc $(CFLAGS) -o message.o -c message.c $(WARNING)

pilha.o: pilha.c pilha.h
	gcc $(CFLAGS) -o pilha.o -c pilha.c $(WARNING)

tokenlizer.o: tokenlizer.c tokenlizer.h
	gcc $(CFLAGS) -o tokenlizer.o -c tokenlizer.c $(WARNING)

lo: CFLAGS += -DNETINTERFACE=\"lo\" 
lo: client server

clean:
	- rm -f *.o *~

purge: clean
	- rm -f client server a.out core
