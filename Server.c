

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
    int read_status = 0;

    printf("Lendo socket...\n");
    for(;;)
    {
        read_status = recv(socket, data_buffer, MESSAGE_SIZE, MSG_TRUNC );
        if( data_buffer[0] == START_FRAME_DELIMITER ){
            printf("%s\n", &data_buffer[1]);
            data_buffer[0] = 0x00;
        }

        if(read_status == -1){
            printf("ERRO NO READ\n    %s\n", strerror(errno));
            exit(-1);
        }
    }
    
    return 0;
}