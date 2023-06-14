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
#include "tokenlizer.h"

#define FILE_BUFFER_SIZE (64 * 1024)

#ifndef NETINTERFACE
#error NETINTERFACE não está definido. Use "make [interface de rede]"
//   definição do macro só pro editor de texto saber que é um macro
#define NETINTERFACE "ERROR"
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
            comando = C_METAMESSAGE;
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
        perror("Ocorreu um erro:\n");
    }
    fprintf(stderr, "\t%s\n", strerror(errno));
}

int open_backup_file(FILE **curr_file, char *filename) {
    struct stat about_file;

    *curr_file = fopen(filename, "r");
    if (*curr_file == NULL)
        return errno;

    if (fstat(fileno(*curr_file), &about_file))
        return errno;

    return 0;
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

        case C_METAMESSAGE:
            return ERRO;

        default:
            return ERRO;
    }  // switch (comando)
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

    void *packets_buffer = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
    empilhar(freeHeap, packets_buffer);
    t_message *messageR = init_message(packets_buffer);  // Você recebe uma resposta. (Pacote recebido)

    int argc;
    char *argv[4096];
    FILE *curr_file = NULL;
    unsigned char *file_buffer = malloc(FILE_BUFFER_SIZE * sizeof(unsigned char));
    empilhar(freeHeap, file_buffer);
    int quantidade_arquivos = 0;
    int arquivos_enviados = 0;
    int estado = PROMPT_DE_COMANDO;
    int comando;
    char error = 0;

    unsigned char type;
    int bytes;

    for (;;) {
        switch (estado) {
            // ==
            case PROMPT_DE_COMANDO:
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
                estado = ERRO;
                if (arquivos_enviados >= quantidade_arquivos) {
                    estado = PROMPT_DE_COMANDO;
                    continue;
                }
                char *filename = argv[arquivos_enviados + 1];
                if (open_backup_file(&curr_file, filename) == 0)
                    if (cm_send_message(socket, filename, strlen(filename) + sizeof((char)'\0'), C_BACKUP_1FILE, messageR) != -1)
                        if (cm_receive_message(socket, &error, 1, &type) != -1) {
                            printf("%d e %d\n", type, type);
                            if (type == C_OK)
                                estado = ENVIANDO_BACKUP_FILE;
                        }
                arquivos_enviados += 1;
                break;

            // ==
            case ENVIANDO_BACKUP_FILE:
                estado = ERRO;
                if (feof(curr_file)) {
                    if (cm_send_message(socket, file_buffer, 0, C_END_OF_FILE, messageR) != -1)
                        if (cm_receive_message(socket, &error, 1, &type) != -1)
                            if (type == C_OK) {  // Enviou todo o arquivo.
                                estado = EXECUTANDO_BACKUP;
                                fprintf(stdout, "\033[%dA\033[0K\r", quantidade_arquivos - arquivos_enviados + 1);
                                fprintf(stdout, "  \"%s\" \033[0;32m(Enviado)", argv[arquivos_enviados]);
                                fprintf(stdout, "\033[%dB\033[0K\r\033[0m", quantidade_arquivos - arquivos_enviados + 1);
                                fflush(stdout);
                            }
                    break;
                }

                bytes = fread(file_buffer, 1, FILE_BUFFER_SIZE, curr_file);
                printf("pedaço: %d\n", bytes);
                if (cm_send_message(socket, file_buffer, bytes, C_DATA, messageR) != -1)
                    estado = ENVIANDO_BACKUP_FILE;

                if (estado == ERRO) {
                    fprintf(stdout, "\033[%dA\033[0K\r", quantidade_arquivos - arquivos_enviados + 1);
                    fprintf(stdout, "  \"%s\" \033[1;31m(ERRO)", argv[arquivos_enviados]);
                    fprintf(stdout, "\033[%dB\033[0K\r\033[0m", quantidade_arquivos - arquivos_enviados + 1);
                    fflush(stdout);
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
                fprintf(stderr, "Ocorreu um erro.\n\t%s\n", strerror(errno));
                estado = PROMPT_DE_COMANDO;
                continue;
        }  // switch(estado)

    }  // for(;;)

    exit(0);
}
