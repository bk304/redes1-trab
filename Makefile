all: client server

WARNING = -Wall -Wextra

client: Client.c ConexaoRawSocket message
	gcc -o client Client.c ConexaoRawSocket.o message.o $(WARNING)

server: Server.c ConexaoRawSocket message
	gcc -o server Server.c ConexaoRawSocket.o message.o $(WARNING)

ConexaoRawSocket: ConexaoRawSocket.c ConexaoRawSocket.h
	gcc -o ConexaoRawSocket.o -c ConexaoRawSocket.c $(WARNING)

message: message.c message.h
	gcc -o message.o -c message.c $(WARNING)