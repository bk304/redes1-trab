

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
    t_message message;
    set_start_delimiter(&message);
    t_message_data message_data;
    int bytes_written;

    char *hello_world = "Olá.\n";
    strcpy( (char *) &message_data, hello_world );
    set_message(&message, sizeof(hello_world), 0, C_UNUSED_1, &message_data);

    printf("Escrevendo no Socket...\n");
    bytes_written = send(socket, &message, MESSAGE_SIZE_BYTES, 0);

    if(bytes_written == -1){
        printf("ERRO NO WRITE\n    %s\n", strerror(errno));
        exit(-1);
    }
    else{
        printf("%d bytes escritos.\n", bytes_written);
    }

    return 0;
}