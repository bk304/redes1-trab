

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

int identifica_comando(char *arg, int qnt) {
    switch (hash_function(arg)) {
        case BACKUP:
            if (qnt == 2)
                return C_BACKUP_1FILE;
            else if (qnt > 2)
                return C_BACKUP_GROUP;
            else
                return 0;

        case REC:
            if (qnt == 2)
                return C_BACKUP_1FILE;
            else if (qnt > 2)
                return C_BACKUP_GROUP;
            else
                return 0;

        case CDSV:
            return CDSV;

        case VERIFICAR:
            return VERIFICAR;

        case CD:
            return CD;

        default:
            return 0;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 1) {
        printf("Modo de Uso:\n\t$ client [backup|rec|cdsv|verificar|cd] [arquivos]\n");
        exit(0);
    }

    int socket = ConexaoRawSocket(NETINTERFACE);
    void *packet_buffer = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
    t_message *message = init_message(packet_buffer);
    int bytes_written;

    int estado = PROMPT_DE_COMANDO;
    int comando = 0;

    for (;;) {
        // identifica o comando
        switch (hash_function(argv[1])) {
            case BACKUP:
                if (argc == 2)
                    comando = C_BACKUP_1FILE;
                else if (argc > 2)
                    comando = C_BACKUP_GROUP;
                else
                    comando = 0;

            case REC:
                if (argc == 2)
                    comando = C_BACKUP_1FILE;
                else if (argc > 2)
                    comando = C_BACKUP_GROUP;
                else
                    comando = 0;

            case CDSV:
                comando = CDSV;

            case VERIFICAR:
                comando = VERIFICAR;

            case CD:
                comando = CD;

            default:
                comando = 0;

                if (!(comando = identifica_comando(argv[1], argc))) {
                    exit(0);
                    // continue;
                }

                printf("%d\n", comando);
                break;
        }
        exit(0);

        // bytes_written = send_message(socket, message, 0, C_UNUSED_1, (void *)str, strlen(str));

        if (bytes_written == -1) {
            printf("ERRO NO WRITE\n    %s\n", strerror(errno));
            exit(-1);
        } else {
            printf("%d bytes escritos.\n", bytes_written);
        }

        return 0;
    }