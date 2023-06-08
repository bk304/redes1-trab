

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
#error NETINTERFACE não está definido. Use "make [interface de rede]"
//  definição do macro só pro editor de texto saber que é um macro
#define NETINTERFACE "lo"
#endif

enum comandos {
    BACKUP = 14,
    REC = 116,
    CDSV = 2,
    VERIFICAR = 119,
    CD = 7
};

enum estados {
    PROMPT_DE_COMANDO,
    ENVIANDO_BACKUP_FILE,
    ENVIANDO_BACKUP_GROUP,
    RECEBENDO_REC_FILE,
    RECEBENDO_REC_GROUP,
    ESPERANDO_MD5  // Comando de verificação
};

unsigned long hash_function(char *str) {
    unsigned long hash_value = 0;
    for (int j = 0; str[j] != '\0'; j++)
        hash_value ^= (unsigned long)str[j];
    return hash_value;
}

void printModoDeUso(void) {
    printf("Modo de Uso:\n\t$ client [backup|rec|cdsv|verificar|cd] [arquivos]\n");
}

int identifica_comando(char *argv[], int argc) {
    int comando = -1;

    switch (hash_function(argv[1])) {
        case BACKUP:
            if (argc == 3)
                comando = C_BACKUP_1FILE;
            else if (argc > 3)
                comando = C_BACKUP_GROUP;
            else
                comando = -1;
            break;

        case REC:
            if (argc == 3)
                comando = C_RECOVER_1FILE;
            else if (argc > 3)
                comando = C_RECOVER_GROUP;
            else
                comando = -1;
            break;

        case CDSV:
            comando = C_CD_SERVER;
            break;

        case VERIFICAR:
            comando = C_VERIFY;
            break;

        case CD:
            comando = C_UNUSED_2;
            break;

        default:
            comando = -1;
            break;
    }

    return comando;
}

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        printModoDeUso();
        exit(0);
    }

    int socket = ConexaoRawSocket(NETINTERFACE);

    void *packets_buffer = malloc(2 * PACKET_SIZE_BYTES * sizeof(unsigned char));
    void *send_packet_buffer = packets_buffer;
    void *receive_packet_buffer = packets_buffer + PACKET_SIZE_BYTES;

    t_message *message = init_message(send_packet_buffer);      // Você envia uma mensagem. (Pacote enviado)
    t_message *response = init_message(receive_packet_buffer);  // Você recebe uma resposta. (Pacote recebido)

    int bytes_sent;
    int bytes_received;
    FILE *curr_file = NULL;
    unsigned long curr_seq = 0;
    int estado = PROMPT_DE_COMANDO;
    int comando = identifica_comando(argv, argc);
    if (comando == -1) {
        printModoDeUso();
        exit(-1);
    }

    for (char breakLoop = 0; !breakLoop;) {
        switch (estado) {
            //
            case PROMPT_DE_COMANDO:

                switch (comando) {
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

                    case C_UNUSED_2:
                        break;

                    default:
                        break;
                }  // switch (comando)

                break;  // case PROMPT_DE_COMANDO:

            //
            case ENVIANDO_BACKUP_FILE:
                break;

            //
            case ENVIANDO_BACKUP_GROUP:
                break;

            //
            case RECEBENDO_REC_FILE:
                break;

            //
            case RECEBENDO_REC_GROUP:
                break;

            //
            case ESPERANDO_MD5:
                break;

        }  // switch (estado)

        bytes_sent = send_message(socket, message);
        if (bytes_sent == -1) {
            printf("ERRO NO WRITE\n    %s\n", strerror(errno));
            exit(-1);
        }

        bytes_received = receive_message(socket, response);
        if (bytes_received == -1) {
            printf("ERRO NO READ\n    %s\n", strerror(errno));
            exit(-1);
        }
        // Essa resposta recebida deve ser tratada sendo da função especifica do caso.

    }  // for (;;)

    free(packets_buffer);
    exit(0);
}
