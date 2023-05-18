all: conC conS

conC: Client.c ConexaoRawSocket
	gcc -o conC Client.c ConexaoRawSocket.o

conS: Server.c ConexaoRawSocket
	gcc -o conS Server.c ConexaoRawSocket.o

ConexaoRawSocket: ConexaoRawSocket.c ConexaoRawSocket.h
	gcc -o ConexaoRawSocket.o -c ConexaoRawSocket.c