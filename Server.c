

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
    int read_status = 0;

    printf("Lendo socket...\n");
    for(;;)
    {
        read_status = recv(socket, &message, MESSAGE_SIZE_BYTES, 0 );
        // if( data_buffer[0] == START_FRAME_DELIMITER ){
        //     printf("%s\n", &data_buffer[1]);
        //     data_buffer[0] = 0x00;
        // }
        printf(":%d\n", read_status);
        printf("%s\n", (char *) &message.data);
        memset(&message.data, 0, DATA_MAX_SIZE_BYTES);

        if(read_status == -1){
            printf("ERRO NO READ\n    %s\n", strerror(errno));
            exit(-1);
        }
    }
    
    return 0;
}