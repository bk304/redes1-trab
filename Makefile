.PHONY: all debug lo clean purge

all: client server

debug: CFLAGS += -DDEBUG 
debug: client server

WARNING = -Wall -Wextra

PARAMS = 

# Alvo "lo"
ifeq ($(filter lo,$(MAKECMDGOALS)),lo)
CFLAGS += -DLO
endif

# Alvo "debug"
ifeq ($(filter debug,$(MAKECMDGOALS)),debug)
CFLAGS += -DDEBUG
endif

RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
$(eval $(RUN_ARGS):;@:)

client: Client.c ConexaoRawSocket.o message.o
	gcc $(CFLAGS) -o client Client.c ConexaoRawSocket.o message.o $(WARNING)

server: Server.c ConexaoRawSocket.o message.o
	gcc $(CFLAGS) -o server Server.c ConexaoRawSocket.o message.o $(WARNING)

ConexaoRawSocket.o: ConexaoRawSocket.c ConexaoRawSocket.h
	gcc $(CFLAGS) -o ConexaoRawSocket.o -c ConexaoRawSocket.c $(WARNING)

message.o: message.c message.h
	gcc $(CFLAGS) -o message.o -c message.c $(WARNING)

lo: CFLAGS += -DNETINFERFACE='lo' 
lo: client server

clean:
	- rm -f *.o *~

purge: clean
	- rm -f client server a.out core
