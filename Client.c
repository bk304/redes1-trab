

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
    int bytes_written;

    char *str = "Olá. Isso é um teste.\n";

    printf("Escrevendo no Socket...\n");
    bytes_written = send_message(socket, message, 0, C_UNUSED_1, (void *)str, strlen(str));

    if (bytes_written == -1) {
        printf("ERRO NO WRITE\n    %s\n", strerror(errno));
        exit(-1);
    } else {
        printf("%d bytes escritos.\n", bytes_written);
    }

    return 0;
}