

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "ConexaoRawSocket.h"
#include "ethernet.h"
#include "message.h"

#define PACKET_MAX_SIZE 65536  // 64 MBytes

int main(void) {
    int socket = ConexaoRawSocket("lo");
    void *packet = malloc(PACKET_MAX_SIZE * sizeof(unsigned char));
    t_ethernet_frame *ethernet_packet = (t_ethernet_frame *)packet;
    t_message *message = (t_message *)&ethernet_packet->payload;
    int read_status = 0;

    printf("Lendo socket...\n");
    for (;;) {
        read_status = recv(socket, message, MESSAGE_SIZE_BYTES, 0);
        // if( data_buffer[0] == START_FRAME_DELIMITER ){
        //     printf("%s\n", &data_buffer[1]);
        //     data_buffer[0] = 0x00;
        // }
        printf(":%d\n", read_status);
        printf("%s\n", (char *)&message->data);
        memset(&message->data, 0, DATA_MAX_SIZE_BYTES);

        if (read_status == -1) {
            printf("ERRO NO READ\n    %s\n", strerror(errno));
            exit(-1);
        }
    }

    return 0;
}