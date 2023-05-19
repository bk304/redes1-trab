all: client server

client: Client.c ConexaoRawSocket
	gcc -o client Client.c ConexaoRawSocket.o

server: Server.c ConexaoRawSocket
	gcc -o server Server.c ConexaoRawSocket.o

ConexaoRawSocket: ConexaoRawSocket.c ConexaoRawSocket.h
	gcc -o ConexaoRawSocket.o -c ConexaoRawSocket.c