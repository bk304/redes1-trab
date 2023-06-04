#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "ConexaoRawSocket.h"
#include "message.h"

int main(void) {
    int socket = ConexaoRawSocket("lo");
    void *packet_buffer = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
    t_message *message = init_message(packet_buffer);
    int read_status = 0;

    printf("Lendo socket...\n");
    for (;;) {
        read_status = receive_message(socket, message);

        if (read_status == -1) {
            printf("ERRO NO READ\n    %s\n", strerror(errno));
            exit(-1);
        }

        switch (message->type) {
            case C_BACKUP_1FILE:
                break;
            case C_BACKUP_GROUP:
                break;
            case C_RECOVER_1FILE:
                break;
            case C_RECOVER_GROUP:
                break;
            case C_CD_SERVER:
                break;
            case C_VERIFY:
                break;
            case C_FILE_NAME:
                break;
            case C_UNUSED_1:
                break;
            case C_DATA:
                break;
            case C_END_OF_FILE:
                break;
            case C_END_OF_GROUP:
                break;
            case C_UNUSED_2:
                break;
            case C_ERROR:
                break;
            case C_OK:
                break;
            case C_ACK:
                break;
            case C_NACK:
                break;
            default:
                continue;
        }

        // ação

        // envia resposta
        memset(packet, 0x00, read_status);
    }

    return 0;
}