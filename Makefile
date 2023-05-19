all: client server

client: Client.c ConexaoRawSocket message
	gcc -o client Client.c ConexaoRawSocket.o message.o

server: Server.c ConexaoRawSocket message
	gcc -o server Server.c ConexaoRawSocket.o message.o

ConexaoRawSocket: ConexaoRawSocket.c ConexaoRawSocket.h
	gcc -o ConexaoRawSocket.o -c ConexaoRawSocket.c

message: message.c message.h
	gcc -o message.o -c message.c