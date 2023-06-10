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
#include "message.h"
#include "pilha.h"

#ifndef NETINTERFACE
#error "NETINTERFACE não está definido. Use 'make [interface de rede]"
// define só para o editor de texto saber que é um macro.
#define NETINTERFACE "lo"
#endif

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

void envia_e_espera(int socketFD, t_message *messageT, t_message *messageR, t_message *messageA) {
    int bytes_sent;
    int bytes_received;
    int parityError = 0;

    do {
        if (!parityError) {
            bytes_sent = send_message(socketFD, messageT);
            if (bytes_sent == -1) {
                printf("ERRO NO WRITE\n");
                exit(-1);
            }
        } else {
            if (send_nack(socketFD, messageA) == -1) {
                printf("ERRO NO WRITE\n");
                exit(-1);
            }
        }

        bytes_received = receive_message(socketFD, messageR);
        if (bytes_received == -1) {
            printf("ERRO NO READ\n");
            exit(-1);
        }
        parityError = (bytes_received == -2);
    } while (messageR->type == C_NACK || parityError);
}

int main(void) {
    type_pilha *freeHeap = criar_pilha(sizeof(void *));
    on_exit(libera_e_sai, freeHeap);

    chdir("./sv");

    int socket = ConexaoRawSocket(NETINTERFACE);

    void *packets_buffer = malloc(3 * PACKET_SIZE_BYTES * sizeof(unsigned char));
    empilhar(freeHeap, packets_buffer);
    t_message *messageT = init_message(packets_buffer);                          // Você envia uma mensagem. (Pacote enviado)
    t_message *messageR = init_message(packets_buffer + PACKET_SIZE_BYTES);      // Você recebe uma resposta. (Pacote recebido)
    t_message *messageA = init_message(packets_buffer + 2 * PACKET_SIZE_BYTES);  // Usado apenas para enviar mensagens de ACK e NACK

    int bytes_written = 0;
    int read_status = 0;
    int estado = ESPERANDO_COMANDO;
    FILE *curr_file = NULL;
    unsigned char curr_seq = 0;

    printf("Lendo socket...\n");
    // Primeira mensagem.
    read_status = receive_message(socket, messageR);
    if (read_status == -1) {
        printf("ERRO NO READ");
        exit(-1);
    } else if (read_status == -2) {
        if (send_nack(socket, messageA) == -1)
            exit(-1);
    }

    for (char breakLoop = 0; !breakLoop;) {
        switch (estado) {
            // ==
            case ESPERANDO_COMANDO:
                curr_seq = 0;
                switch (messageR->type) {
                    case C_BACKUP_1FILE:
                        printf("Iniciando backup do arquivo \"%s\".\n", messageR->data);
                        curr_file = fopen((char *)messageR->data, "w+");

                        if (curr_file == NULL) {
                            estado = ERRO;
                            messageT->type = C_ERROR;
                            if (errno == EACCES)
                                messageT->data[0] = NO_WRITE_PERMISSION;
                            else if (errno == EDQUOT)
                                messageT->data[0] = DISK_FULL;
                            else
                                messageT->data[0] = UNKNOW_ERROR;
                        } else {
                            estado = RECEBENDO_BACKUP_FILE;
                            messageT->type = C_OK;
                        }

                        messageT->length = 0;
                        messageT->sequence = 0;
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
                switch (messageR->type) {
                    case C_DATA:
                        fwrite(messageR->data, 1, messageR->length, curr_file);
                        messageT->type = C_ACK;
                        messageT->length = 0;
                        messageT->sequence = messageR->sequence;
                        break;
                    case C_END_OF_FILE:
                        fclose(curr_file);
                        messageT->type = C_OK;
                        messageT->length = 0;
                        messageT->sequence = messageR->sequence;
                        printf("Backup Concluido.\n");
                        estado = ESPERANDO_COMANDO;
                        break;
                    default:
                        estado = ERRO;
                        break;
                }       // switch (messageT->type)
                break;  // case RECEBENDO_BACKUP_FILE

            // ==
            case RECEBENDO_BACKUP_GROUP:
                switch (messageT->type) {
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
                }       // switch (messageT->type)
                break;  // case RECEBENDO_BACKUP_GROUP

            // ==
            case ENVIANDO_REC_FILE:
                switch (messageT->type) {
                    case C_ACK:
                        break;
                    case C_NACK:
                        break;
                    default:
                        break;
                }       // switch (messageT->type)
                break;  // case ENVIANDO_REC_FILE

            // ==
            case ENVIANDO_REC_GROUP:
                switch (messageT->type) {
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
                }       // switch (messageT->type)
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

        envia_e_espera(socket, messageT, messageR, messageA);

    }  // for (;;)

    exit(0);
}