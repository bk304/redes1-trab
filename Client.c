

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
    memset(ethernet_packet->mac_destination, 0x00, 6);
    memset(ethernet_packet->mac_source, 0x00, 6);
    ethernet_packet->len_or_type[0] = 0x73;
    ethernet_packet->len_or_type[1] = 0x04;
    t_message *message = (t_message *)&ethernet_packet->payload;
    set_start_delimiter(message);
    t_message_data message_data;
    int bytes_written;

    char *hello_world = "Olá.\n";
    strcpy((char *)&message_data, hello_world);
    set_message(message, sizeof(hello_world), 0, C_UNUSED_1, &message_data);

    printf("Escrevendo no Socket...\n");
    bytes_written = send(socket, message, MESSAGE_SIZE_BYTES, 0);

    if (bytes_written == -1) {
        printf("ERRO NO WRITE\n    %s\n", strerror(errno));
        exit(-1);
    } else {
        printf("%d bytes escritos.\n", bytes_written);
    }

    return 0;
}