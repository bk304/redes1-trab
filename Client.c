#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
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
#include "slidingWindow.h"
#include "tokenlizer.h"

#ifndef NETINTERFACE
#error NETINTERFACE não está definido. Use "make [interface de rede]"
//   definição do macro só pro editor de texto saber que é um macro
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
    EXECUTANDO_BACKUP,
    RECEBENDO_REC_FILE,
    RECEBENDO_REC_GROUP,
    ESPERANDO_MD5,  // Comando de verificação
    ERRO,
    EXIT
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

int is_regular_file(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

int identifica_comando(char *argv[], int argc) {
    int comando = -1;

    switch (hash_function(argv[0])) {
        case BACKUP:
            if (argc == 2)
                comando = C_BACKUP_1FILE;
            else if (argc > 2)
                comando = C_BACKUP_GROUP;
            else
                comando = -1;
            break;

        case REC:
            if (argc == 2)
                comando = C_RECOVER_1FILE;
            else if (argc > 2)
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
            comando = C_UNUSED;
            break;

        default:
            comando = -1;
            break;
    }

    return comando;
}

void libera_e_sai(int exitCode, void *freeHeap) {
    void *ptr = NULL;

    while (desempilhar((type_pilha *)freeHeap, &ptr)) {
        free(ptr);
    }
    destruir_pilha((type_pilha *)freeHeap);

    if (exitCode == -1) {
        printf("Ocorreu um erro.\n");
    }
    printf("\t%s\n", strerror(errno));
}

int interpreta_comando(int comando) {
    switch (comando) {
        case C_BACKUP_1FILE:
        case C_BACKUP_GROUP:
            return EXECUTANDO_BACKUP;

        case C_RECOVER_1FILE:
            return ERRO;

        case C_RECOVER_GROUP:
            return ERRO;

        case C_CD_SERVER:
            return ERRO;

        case C_VERIFY:
            return ERRO;

        case C_UNUSED:
            return ERRO;

        default:
            return ERRO;
    }  // switch (comando)
}

int f_backup_file(t_message *messageT, FILE **curr_file, char *filename, unsigned char *total_seq) {
    struct stat about_file;

    *curr_file = fopen(filename, "r");
    if (*curr_file == NULL)
        return errno;

    if (fstat(fileno(*curr_file), &about_file))
        return errno;

    *total_seq = (about_file.st_size + DATA_MAX_SIZE_BYTES - 1) / DATA_MAX_SIZE_BYTES;

    messageT->length = strlen(filename) + sizeof((char)'\0');
    messageT->sequence = 0;
    messageT->type = C_BACKUP_1FILE;
    strcpy((char *)messageT->data, filename);

    return 0;
}

int mensagem_backup(t_message *messageT, FILE *curr_file, unsigned char curr_seq) {
    if (feof(curr_file)) {
        messageT->type = C_END_OF_FILE;
        messageT->sequence = curr_seq;
        messageT->length = 0;
        return EXECUTANDO_BACKUP;
    } else {
        messageT->type = C_DATA;
        messageT->sequence = curr_seq;
        messageT->length = fread(messageT->data, 1, DATA_MAX_SIZE_BYTES, curr_file);
        return ENVIANDO_BACKUP_FILE;
    }
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

int le_comando(int *argc, char *argv[], int *qnt_arquivos) {
    char input[4096];
    size_t len;

    fprintf(stdout, "\t>>> ");
    fgets(input, 4096, stdin);
    len = strlen(input);
    if (len > 0 && input[len - 1] == '\n')
        input[--len] = '\0';
    if (tokenlize(input, argc, argv)) {
        exit(-1);
    }

    *qnt_arquivos = *argc - 1;
    return identifica_comando(argv, *argc);
}

int main(void) {
    type_pilha *freeHeap = criar_pilha(sizeof(void *));
    on_exit(libera_e_sai, freeHeap);

    // if (argc <= 1) {
    //     printModoDeUso();
    //     exit(0);
    // }

    int socket = ConexaoRawSocket(NETINTERFACE);

    void *packets_buffer = malloc(3 * PACKET_SIZE_BYTES * sizeof(unsigned char));
    empilhar(freeHeap, packets_buffer);
    t_message *messageT = init_message(packets_buffer);                          // Você envia uma mensagem. (Pacote enviado)
    t_message *messageR = init_message(packets_buffer + PACKET_SIZE_BYTES);      // Você recebe uma resposta. (Pacote recebido)
    t_message *messageA = init_message(packets_buffer + 2 * PACKET_SIZE_BYTES);  // Usado apenas para enviar mensagens de ACK e NACK

    pthread_t connectionManagerThread;
    pthread_create(&connectionManagerThread, NULL, cm_init, (void *)window);

    int argc;
    char *argv[4096];
    FILE *curr_file = NULL;
    int quantidade_arquivos = 0;
    int arquivos_enviados = 0;
    unsigned char curr_seq = 0;
    unsigned char total_seq = 0;
    int estado = PROMPT_DE_COMANDO;
    int comando;

    for (;;) {
        switch (estado) {
            // ==
            case PROMPT_DE_COMANDO:
                curr_seq = 0;
                comando = le_comando(&argc, argv, &quantidade_arquivos);
                if (comando == -1) {
                    printModoDeUso();
                    break;
                }

                estado = interpreta_comando(comando);
                if (estado == EXECUTANDO_BACKUP) {
                    for (int i = 1; i < argc; i++) {
                        if (access(argv[i], R_OK) != 0) {
                            printf("  \"%s\" \033[1;31m(Arquivo inexistante ou sem permissão de leitura)\033[0m\n", argv[i]);
                            estado = PROMPT_DE_COMANDO;
                        } else if (!is_regular_file(argv[i])) {
                            printf("  \"%s\" \033[1;31m(Arquivo não é um arquivo regular)\033[0m\n", argv[i]);
                            estado = PROMPT_DE_COMANDO;
                        } else {
                            printf("  \"%s\"\n", argv[i]);
                        }
                    }
                }

                if (estado == PROMPT_DE_COMANDO) {
                    printf("\033[1;31mOperação abortada.\033[0m\n");
                }
                break;

            // ==
            case EXECUTANDO_BACKUP:
                if (arquivos_enviados >= quantidade_arquivos) {
                    estado = PROMPT_DE_COMANDO;
                    continue;
                }
                if (f_backup_file(messageT, &curr_file, argv[arquivos_enviados + 1], &total_seq) == 0) {
                    envia_e_espera(socket, messageT, messageR, messageA);
                    if (messageR->type == C_OK) {
                        estado = ENVIANDO_BACKUP_FILE;
                    } else
                        estado = ERRO;
                } else
                    estado = ERRO;
                arquivos_enviados += 1;
                break;

            // ==
            case ENVIANDO_BACKUP_FILE:
                curr_seq++;
                estado = mensagem_backup(messageT, curr_file, curr_seq);
                envia_e_espera(socket, messageT, messageR, messageA);
                if (messageR->type == C_ACK) {
                    // mantem o estado
                } else if (messageR->type == C_OK) {  // Enviou todo o arquivo.
                    estado = EXECUTANDO_BACKUP;
                    fprintf(stdout, "\033[%dA\033[0K\r", quantidade_arquivos - arquivos_enviados + 1);
                    fprintf(stdout, "  \"%s\" \033[0;32m(Enviado)", argv[arquivos_enviados]);
                    fprintf(stdout, "\033[%dB\033[0K\r\033[0m", quantidade_arquivos - arquivos_enviados + 1);
                    fflush(stdout);
                } else {
                    fprintf(stdout, "\033[%dA\033[0K\r", quantidade_arquivos - arquivos_enviados + 1);
                    fprintf(stdout, "  \"%s\" \033[1;31m(ERRO)", argv[arquivos_enviados]);
                    fprintf(stdout, "\033[%dB\033[0K\r\033[0m", quantidade_arquivos - arquivos_enviados + 1);
                    fflush(stdout);
                    estado = ERRO;
                }
                break;

            // ==
            case RECEBENDO_REC_FILE:
                break;

            // ==
            case RECEBENDO_REC_GROUP:
                break;

            // ==
            case ESPERANDO_MD5:
                break;

            // ==
            case EXIT:
                exit(0);

            // ==
            case ERRO:
            default:
                printf("Ocorreu um erro.\n\t%s\n", strerror(errno));
                estado = PROMPT_DE_COMANDO;
                continue;
        }  // switch(estado)

    }  // for(;;)

    exit(0);
}
