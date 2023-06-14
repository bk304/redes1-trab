#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ConexaoRawSocket.h"
#include "connectionManager.h"
#include "message.h"
#include "pilha.h"

#ifndef NETINTERFACE
#error "NETINTERFACE não está definido. Use 'make [interface de rede]"
// define só para o editor de texto saber que é um macro.
#define NETINTERFACE "lo"
#endif

#define FILE_BUFFER_SIZE (64 * 1024)

enum estados {
    ESPERANDO_COMANDO,
    RECEBENDO_BACKUP_FILE,
    RECEBENDO_BACKUP_GROUP,
    ENVIANDO_REC_FILE,
    ENVIANDO_REC_GROUP,
    ERRO,
    EXIT
};

void libera_e_sai(__attribute__((unused)) int exitCode, void *freeHeap) {
    void *ptr = NULL;
    while (desempilhar((type_pilha *)freeHeap, &ptr)) {
        free(ptr);
    }

    destruir_pilha((type_pilha *)freeHeap);
    printf("%s\n", strerror(errno));
}

int main(void) {
    type_pilha *freeHeap = criar_pilha(sizeof(void *));
    on_exit(libera_e_sai, freeHeap);

    chdir("./sv");

    int socket = ConexaoRawSocket(NETINTERFACE);

    void *packets_buffer = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
    empilhar(freeHeap, packets_buffer);
    t_message *messageR = init_message(packets_buffer);  // Você recebe uma resposta. (Pacote recebido)

    int estado = ESPERANDO_COMANDO;
    FILE *curr_file = NULL;
    unsigned char *file_buffer = malloc(FILE_BUFFER_SIZE * sizeof(unsigned char));
    empilhar(freeHeap, file_buffer);

    char error = 0;

    unsigned char typeR;
    unsigned char typeT;
    int bytesR;
    int bytesT;

    printf("Lendo socket...\n");
    // Primeira mensagem.
    if ((bytesR = cm_receive_message(socket, file_buffer, FILE_BUFFER_SIZE, &typeR)) == -1)
        exit(-1);

    printf("Type: %d\n", typeR);

    for (;;) {
        switch (estado) {
            // ==
            case ESPERANDO_COMANDO:
                switch (typeR) {
                    case C_BACKUP_1FILE:
                        printf("Iniciando backup do arquivo \"%s\".\n", file_buffer);
                        curr_file = fopen((char *)file_buffer, "w+");
                        if (curr_file == NULL) {
                            estado = ERRO;
                            typeT = C_ERROR;
                            bytesT = 1;
                            if (errno == EACCES)
                                ((messageError *)file_buffer)->errorCode = NO_WRITE_PERMISSION;
                            else if (errno == EDQUOT)
                                ((messageError *)file_buffer)->errorCode = DISK_FULL;
                            else
                                ((messageError *)file_buffer)->errorCode = UNKNOW_ERROR;
                        } else {
                            estado = RECEBENDO_BACKUP_FILE;
                            typeT = C_OK;
                            bytesT = 0;
                        }
                        if (cm_send_message(socket, file_buffer, bytesT, typeT, messageR) == -1)
                            exit(-1);
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
                }       // switch (messageT->type)
                break;  // case ESPERANDO_COMANDO

            // ==
            case RECEBENDO_BACKUP_FILE:
                switch (typeR) {
                    case C_DATA:
                        fwrite(file_buffer, 1, FILE_BUFFER_SIZE, curr_file);
                        estado = RECEBENDO_BACKUP_FILE;
                        break;
                    case C_END_OF_FILE:
                        fclose(curr_file);
                        printf("Backup Concluido.\n");
                        estado = ESPERANDO_COMANDO;
                        break;
                    default:
                        estado = ERRO;
                        break;
                }       // switch (typeR)
                break;  // case RECEBENDO_BACKUP_FILE

            // ==
            case RECEBENDO_BACKUP_GROUP:
                switch (typeR) {
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
                }       // switch (typeR)
                break;  // case RECEBENDO_BACKUP_GROUP

            // ==
            case ENVIANDO_REC_FILE:
                switch (typeR) {
                    case C_ACK:
                        break;
                    case C_NACK:
                        break;
                    default:
                        break;
                }       // switch (typeR)
                break;  // case ENVIANDO_REC_FILE

            // ==
            case ENVIANDO_REC_GROUP:
                switch (typeR) {
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
                }       // switch (typeR)
                break;  // case ENVIANDO_REC_GROUP

            // ==
            case EXIT:
                exit(0);

            // ==
            case ERRO:
            default:
                printf("Ocorreu um erro.\n\t%s\n", strerror(errno));
                estado = ESPERANDO_COMANDO;
                continue;
        }  // switch (estado)

        if ((bytesR = cm_receive_message(socket, file_buffer, FILE_BUFFER_SIZE, &typeR)) == -1)
            exit(-1);

    }  // for (;;)

    exit(0);
}