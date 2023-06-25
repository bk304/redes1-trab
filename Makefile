.PHONY: all debug lo clean purge

all: client server

WARNING = -Wall -Wextra -Wno-packed-bitfield-compat

LFLAGS = -pthread -g
CFLAGS = -g

# Alvo "debug"
ifeq ($(filter debug,$(MAKECMDGOALS)),debug)
CFLAGS += -DDEBUG
endif

# Alvo "showDeleted"
ifeq ($(filter showDeleted,$(MAKECMDGOALS)),showDeleted)
CFLAGS += -DSHOWDELETED="TRUE"
endif

RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS)) \
	$(eval $(RUN_ARGS):;@:)

client: Client.c ConexaoRawSocket.o message.o pilha.o tokenlizer.o connectionManager.o gistfile1.o
	gcc $(CFLAGS) $(LFLAGS) -o client Client.c ConexaoRawSocket.o message.o pilha.o tokenlizer.o connectionManager.o gistfile1.o $(WARNING)

server: Server.c ConexaoRawSocket.o message.o pilha.o tokenlizer.o connectionManager.o gistfile1.o
	gcc $(CFLAGS) $(LFLAGS) -o server Server.c ConexaoRawSocket.o message.o pilha.o tokenlizer.o connectionManager.o gistfile1.o $(WARNING)

ConexaoRawSocket.o: ConexaoRawSocket.c ConexaoRawSocket.h
	gcc $(CFLAGS) -o ConexaoRawSocket.o -c ConexaoRawSocket.c $(WARNING)

message.o: message.c message.h
	gcc $(CFLAGS) -o message.o -c message.c $(WARNING)

pilha.o: pilha.c pilha.h
	gcc $(CFLAGS) -o pilha.o -c pilha.c $(WARNING)

tokenlizer.o: tokenlizer.c tokenlizer.h
	gcc $(CFLAGS) -o tokenlizer.o -c tokenlizer.c $(WARNING)

gistfile1.o: gistfile1.c gistfile1.h
	gcc $(CFLAGS) -o gistfile1.o -c gistfile1.c $(WARNING)

connectionManager.o: connectionManager.c connectionManager.h message.h
	gcc $(CFLAGS) -o connectionManager.o -c connectionManager.c $(WARNING)

lo: CFLAGS += -DNETINTERFACE=\"lo\" -DLO
lo: all

clean:
	- rm -f *.o *~

purge: clean
	- rm -f client server a.out core
