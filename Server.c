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

#ifndef NETINTERFACE
#define NETINTERFACE "lo"
#endif

enum estados {
    ESPERANDO_COMANDO,
    BACKUP_FILE,
    BACKUP_GROUP,
    REC_FILE,
    REC_GROUP
};

int main(void) {
    int socket = ConexaoRawSocket(NETINTERFACE);
    void *packet_buffer = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
    t_message *message = init_message(packet_buffer);
    int read_status = 0;

    int estado = ESPERANDO_COMANDO;

    printf("Lendo socket...\n");
    for (;;) {
        read_status = receive_message(socket, message);

        if (read_status == -1) {
            printf("ERRO NO READ\n    %s\n", strerror(errno));
            exit(-1);
        }

        switch (estado) {
            // ==
            case ESPERANDO_COMANDO:
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
                    default:
                        break;
                }
                break;

            // ==
            case BACKUP_FILE:
                switch (message->type) {
                    case C_DATA:
                        break;
                    case C_END_OF_FILE:
                        break;
                    default:
                        break;
                }
                break;

            // ==
            case BACKUP_GROUP:
                switch (message->type) {
                    case C_FILE_NAME:
                        break;
                    case C_DATA:
                        break;
                    case C_END_OF_FILE:
                        break;
                    case C_END_OF_GROUP:
                        break;
                    default:
                        break;
                }
                break;

            // ==
            case REC_FILE:
                switch (message->type) {
                    case C_ACK:
                        break;
                    case C_NACK:
                        break;
                    default:
                        break;
                }
                break;

            // ==
            case REC_GROUP:
                switch (message->type) {
                    case C_ERROR:
                        break;
                    case C_OK:
                        break;
                    case C_ACK:
                        break;
                    case C_NACK:
                        break;
                    default:
                        break;
                }
                break;

            // ==
            default:
                break;
        }

        // ação
        fprintf(stdout, "Pacote recebido.\n");
        fprintf(stdout, "%s", message->data);

        // envia resposta
        memset(message, 0x00, MESSAGE_SIZE_BYTES);
    }

    return 0;
}