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
#error "NETINTERFACE não está definido. Use 'make [interface de rede]"
// define só para o editor de texto saber que é um macro.
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

    void packets_buffer = malloc(2 * PACKET_SIZE_BYTES * sizeof(unsigned char));
    t_message *message = init_message(packets_buffer);                       // Você envia uma mensagem. (Pacote enviado)
    t_message *response = init_message(packets_buffer + PACKET_SIZE_BYTES);  // Você recebe uma resposta. (Pacote recebido)

    int bytes_written = 0;
    int read_status = 0;
    int estado = ESPERANDO_COMANDO;
    FILE *curr_file = NULL;
    unsigned long curr_seq = 0;

    printf("Lendo socket...\n");
    for (char breakLoop = 0; !breakLoop;) {
        read_status = receive_message(socket, response);
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
                }       // switch (message->type)
                break;  // case ESPERANDO_COMANDO

            // ==
            case BACKUP_FILE:
                switch (message->type) {
                    case C_DATA:
                        break;
                    case C_END_OF_FILE:
                        break;
                    default:
                        break;
                }       // switch (message->type)
                break;  // case BACKUP_FILE

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
                }       // switch (message->type)
                break;  // case BACKUP_GROUP

            // ==
            case REC_FILE:
                switch (message->type) {
                    case C_ACK:
                        break;
                    case C_NACK:
                        break;
                    default:
                        break;
                }       // switch (message->type)
                break;  // case REC_FILE

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
                }       // switch (message->type)
                break;  // case REC_GROUP

            // ==
            default:
                break;
        }  // switch (estado)

        bytes_written = send_message(socket, message);
        if (bytes_written == -1) {
            printf("ERRO NO WRITE\n    %s\n", strerror(errno));
            exit(-1);
        }

    }  // for (;;)

    free(packets_buffer);
    exit(0);
}