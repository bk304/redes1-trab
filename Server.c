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

// enum estados {
//     RECEBENDO_BACKUP,
//     ENVIADO_BACKUP,
//     ESPERANDO_COMANDO
// }

int main(void) {
    int socket = ConexaoRawSocket("lo");
    void *packet_buffer = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
    t_message *message = init_message(packet_buffer);
    int read_status = 0;

    // FILE *curr_file;

    // int estado = ESPERANDO_COMANDO;

    printf("Lendo socket...\n");
    for (;;) {
        read_status = receive_message(socket, message);

        if (read_status == -1) {
            printf("ERRO NO READ\n    %s\n", strerror(errno));
            exit(-1);
        }

        fprintf(stdout, "Pacote recebido.\n");
        fprintf(stdout, "%s", message->data);

        memset(packet_buffer, 0x00, read_status);
    }

    return 0;
}