#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ConexaoRawSocket.h"
#include "connectionManager.h"
#include "gistfile1.h"
#include "message.h"
#include "pilha.h"
#include "tokenlizer.h"

#ifndef NETINTERFACE
#error "NETINTERFACE não está definido. Use 'make [interface de rede]"
// define só para o editor de texto saber que é um macro.
#define NETINTERFACE "lo"
#endif

#define BUFFER_SIZE (64 * 1024 * 1024)

#define SV_ROOT_PATH ("./serverFolder")
#define DEFAULT_PATH ("./serverFolder/backups/default")

enum estados {
    ESPERANDO_COMANDO,
    RECEBENDO_BACKUP_FILE,
    RECEBENDO_BACKUP_GROUP,
    EXECUTANDO_REC,
    ENVIANDO_REC_FILE,
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

    errno = 0;
    if (mkdir_p(DEFAULT_PATH, S_IRUSR | S_IWUSR)) {
        printf("O diretório default não existe e é impossivel criar ele.\n");
        exit(-1);
    }
    // Por questão de segurança decidi que o servidor não deve interagir com pastas fora dessa área.
    chroot(SV_ROOT_PATH);
    chdir(DEFAULT_PATH);

    int socket = ConexaoRawSocket(NETINTERFACE);

    char *argStr = malloc(2 * ARG_MAX_SIZE * sizeof(char));
    empilhar(freeHeap, argStr);

    void *packets_buffer = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
    empilhar(freeHeap, packets_buffer);
    t_message *messageR = init_message(packets_buffer);  // Você recebe uma resposta. (Pacote recebido)

    int estado = ESPERANDO_COMANDO;
    FILE *curr_file = NULL;
    unsigned char *buffer = malloc(BUFFER_SIZE * sizeof(unsigned char));
    empilhar(freeHeap, buffer);
    if (buffer == NULL) {
        printf("Falha ao alocar buffer de arquivo.\n");
        exit(-1);
    }

    int arquivos_processados = 0;
    int qnt_arquivos = 0;
    int argc;
    char *argv[4096];
    int count = 0;

    unsigned char typeR;
    unsigned char typeT;
    int bytesR;
    int bytesT;

    printf("Lendo socket...\n");
    // Primeira mensagem.
    if ((bytesR = cm_receive_message(socket, buffer, BUFFER_SIZE, &typeR)) == -1)
        exit(-1);

    for (;;) {
        switch (estado) {
            // ==
            case ESPERANDO_COMANDO:
                count = 0;
                switch (typeR) {
                    case C_BACKUP_1FILE:
                        printf("Iniciando backup do arquivo \"%s\".\n", buffer);
                        curr_file = fopen(getFileName((char *)buffer), "w+");
                        if (curr_file == NULL) {
                            printf("Falha ao abrir o arquivo. Backup abortado.\n");
                            estado = ERRO;
                            typeT = C_ERROR;
                            bytesT = 1;
                            if (errno == EACCES)
                                ((messageError *)buffer)->errorCode = NO_WRITE_PERMISSION;
                            else if (errno == EDQUOT)
                                ((messageError *)buffer)->errorCode = DISK_FULL;
                            else {
                                bytesT = 2;
                                ((messageError *)buffer)->errorCode = CHECK_ERRNO;
                                ((messageError *)buffer)->errnoCode = errno;
                            }
                        } else {
                            estado = RECEBENDO_BACKUP_FILE;
                            typeT = C_OK;
                            bytesT = 0;
                        }
                        if (cm_send_message(socket, buffer, bytesT, typeT, messageR) == -1)
                            exit(-1);
                        break;

                    case C_BACKUP_GROUP:
                        break;
                    case C_RECOVER_1FILE:
                        // Separa nomes com asterisco (*) em todas as possibilidades
                        if (tokenlize((char *)buffer, &argc, argv)) {
                            exit(-1);
                        }

                        // else if (!is_regular_file(argv[i])) {
                        //     printf("  \"%s\" \033[1;31m(Arquivo não é um arquivo regular)\033[0m\n", argv[i]);
                        //     estado = PROMPT_DE_COMANDO;
                        // }
                        estado = EXECUTANDO_REC;
                        for (int i = 0; i < argc; i++) {
                            if (access(argv[i], R_OK) != 0) {
                                printf("  \"%s\" \033[1;31m(Arquivo inexistante ou sem permissão de leitura)\033[0m\n", argv[i]);
                                estado = ERRO;
                            } else {
                                printf("  \"%s\"\n", argv[i]);
                            }
                        }
                        // No caso de um erro ter ocorrido:
                        if (estado == ERRO) {
                            printf("\033[1;31mOperação abortada.\033[0m\n");
                            typeT = C_ERROR;
                            bytesT = 1;
                            if (errno == EACCES)
                                ((messageError *)buffer)->errorCode = NO_READ_PERMISSION;
                            else if (errno == ENOENT)
                                ((messageError *)buffer)->errorCode = FILE_NOT_FOUND;
                            else {
                                bytesT = 2;
                                ((messageError *)buffer)->errorCode = CHECK_ERRNO;
                                ((messageError *)buffer)->errnoCode = errno;
                            }
                        } else {
                            typeT = C_OK;
                            bytesT = sizeof(int);
                            qnt_arquivos = argc;
                            ((int *)buffer)[0] = qnt_arquivos;
                        }
                        if (cm_send_message(socket, buffer, bytesT, typeT, messageR) == -1)
                            exit(-1);
                        break;

                    case C_RECOVER_GROUP:
                        break;
                    case C_CD_SERVER:
                        printf("Trocando de diretório para: \"%s\".\n", buffer);
                        mkdir_p((char *)buffer, S_IRUSR | S_IWUSR);
                        if (chdir((char *)buffer) != 0) {
                            estado = ERRO;
                            typeT = C_ERROR;
                            bytesT = 2;
                            ((messageError *)buffer)->errorCode = CHECK_ERRNO;
                            ((messageError *)buffer)->errnoCode = errno;
                        } else {
                            estado = ESPERANDO_COMANDO;
                            typeT = C_OK;
                            bytesT = 0;
                        }
                        if (cm_send_message(socket, buffer, bytesT, typeT, messageR) == -1)
                            exit(-1);
                        break;
                    default:
                        break;
                }  // switch (messageT->type)
                if ((bytesR = cm_receive_message(socket, buffer, BUFFER_SIZE, &typeR)) == -1)
                    exit(-1);
                break;  // case ESPERANDO_COMANDO

            // ==
            case RECEBENDO_BACKUP_FILE:
                switch (typeR) {
                    case C_DATA:
                        fwrite(buffer, 1, bytesR, curr_file);
                        estado = RECEBENDO_BACKUP_FILE;
                        break;
                    case C_END_OF_FILE:
                        fclose(curr_file);
                        if (cm_send_message(socket, buffer, 0, C_OK, messageR) == -1)
                            exit(-1);
                        printf("Backup Concluido.\n");
                        estado = ESPERANDO_COMANDO;
                        break;
                    default:
                        estado = ERRO;
                        break;
                }  // switch (typeR)
                if ((bytesR = cm_receive_message(socket, buffer, BUFFER_SIZE, &typeR)) == -1)
                    exit(-1);
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
            case EXECUTANDO_REC:
                estado = ERRO;
                if (count == 0) {
                    // Veio do estado PROMPT_DE_COMANDO para esse
                    count++;
                    arquivos_processados = 0;
                    qnt_arquivos = argc;
                }

                if (arquivos_processados >= qnt_arquivos) {
                    estado = ESPERANDO_COMANDO;
                    break;
                }
                char *filename = argv[arquivos_processados];
                if (open_file(&curr_file, filename) == 0)
                    if (cm_send_message(socket, filename, strlen(filename) + sizeof((char)'\0'), C_FILE_NAME, messageR) != -1)
                        if (cm_receive_message(socket, buffer, BUFFER_SIZE, &typeR) != -1) {
                            if (typeR == C_OK)
                                estado = ENVIANDO_REC_FILE;
                            else {
                                estado = ERRO;
                                errno = ((messageError *)buffer)->errnoCode;
                            }
                        }

                break;

            // ==
            case ENVIANDO_REC_FILE:
                estado = ERRO;
                if (feof(curr_file)) {
                    if (cm_send_message(socket, buffer, 0, C_END_OF_FILE, messageR) != -1)
                        if (cm_receive_message(socket, buffer, 1, &typeR) != -1)
                            if (typeR == C_OK) {  // Enviou todo o arquivo.
                                estado = EXECUTANDO_REC;
                                arquivos_processados += 1;
                                fprintf(stdout, "\033[%dA\033[0K\r", qnt_arquivos - arquivos_processados + 1);
                                fprintf(stdout, "  \"%s\" \033[0;32m(Enviado)", argv[arquivos_processados - 1]);
                                fprintf(stdout, "\033[%dB\033[0K\r\033[0m", qnt_arquivos - arquivos_processados + 1);
                                fflush(stdout);
                            }
                    break;
                }

                int bytesToSend;
                int offset = 0;
                bytesToSend = fread(buffer, 1, BUFFER_SIZE, curr_file);
                for (;;) {
                    if (cm_send_message(socket, buffer + offset, bytesToSend, C_DATA, messageR) != -1) {
                        estado = ENVIANDO_REC_FILE;
                        break;
                    } else {
                        messageError *messageErr = (messageError *)messageR;
                        if (messageErr->type == C_ERROR && messageErr->errorCode == BUFFER_FULL) {
                            offset += messageErr->extraInfo;
                            bytesToSend -= messageErr->extraInfo;
                        } else {
                            printf("type: %d e extra: %d\n", messageErr->type, messageErr->errorCode);
                            estado = ERRO;
                            break;
                        }
                    }
                }

                if (estado == ERRO) {
                    arquivos_processados += 1;
                    fprintf(stdout, "\033[%dA\033[0K\r", qnt_arquivos - arquivos_processados + 1);
                    fprintf(stdout, "  \"%s\" \033[1;31m(ERRO)", argv[arquivos_processados - 1]);
                    fprintf(stdout, "\033[%dB\033[0K\r\033[0m", qnt_arquivos - arquivos_processados + 1);
                    fflush(stdout);
                }
                break;

            // ==
            case EXIT:
                exit(0);

            // ==
            case ERRO:
            default:
                printf("Ocorreu um erro.\n\t%s\n", strerror(errno));
                estado = ESPERANDO_COMANDO;
                if ((bytesR = cm_receive_message(socket, buffer, BUFFER_SIZE, &typeR)) == -1)
                    exit(-1);
                break;
        }  // switch (estado)

    }  // for (;;)

    exit(0);
}