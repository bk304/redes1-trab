

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

#define PACKET_MAX_SIZE 65536  // 64 KBytes

int main(void) {
    int socket = ConexaoRawSocket("lo");
    void *packet = malloc(PACKET_MAX_SIZE * sizeof(unsigned char));
    t_message *message;
    t_ethernet_frame *ethernet_packet;
    int read_status = 0;

    printf("Lendo socket...\n");
    for (;;) {
        read_status = recv(socket, packet, PACKET_MAX_SIZE, 0);

        if(read_status == -1){
            printf("ERRO NO READ\n    %s\n", strerror(errno));
            exit(-1);
        }

        ethernet_packet = (t_ethernet_frame *) packet;

        if( *(short *)(ethernet_packet->len_or_type) != htons(0x7304)){
            continue;
        }

        message = (t_message *)ethernet_packet->payload;

        if(message->start_frame_delimiter != START_FRAME_DELIMITER){
            continue;
        }


        printf("%s\n", message->data);


        memset(packet, 0x00, read_status);
    }

    return 0;
}