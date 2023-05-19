

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "ConexaoRawSocket.h"
#include "message.h"

int main(void){
    int socket = ConexaoRawSocket("lo");
    char data_buffer[MESSAGE_SIZE];
    int bytes_written;

    data_buffer[0] = START_FRAME_DELIMITER;
    char *message_data = "Olá!\n";
    strcpy( &data_buffer[1], message_data );

    printf("Escrevendo no Socket...\n");
    bytes_written = send(socket, data_buffer, MESSAGE_SIZE, 0);

    if(bytes_written == -1){
        printf("ERRO NO WRITE\n    %s\n", strerror(errno));
        exit(-1);
    }
    else{
        printf("%d bytes escritos.\n", bytes_written);
    }

    return 0;
}