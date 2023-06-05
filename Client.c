

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
// #error "NETINTERFACE não está definido. Use "make [interface de rede]"
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

int identifica_comando(char *arg) {
    switch (hash_function(arg)) {
        case HASH_FUNCTION("backup"):
            printf("Foi\n");
            break;

        default:
            printf("Não foi\n");
            break;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int socket = ConexaoRawSocket(NETINTERFACE);
    void *packet_buffer = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
    t_message *message = init_message(packet_buffer);
    int bytes_written;

    int estado = PROMPT_DE_COMANDO;

    for (;;) {
    }

    // bytes_written = send_message(socket, message, 0, C_UNUSED_1, (void *)str, strlen(str));

    if (bytes_written == -1) {
        printf("ERRO NO WRITE\n    %s\n", strerror(errno));
        exit(-1);
    } else {
        printf("%d bytes escritos.\n", bytes_written);
    }

    return 0;
}