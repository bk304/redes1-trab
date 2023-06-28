#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <openssl/md5.h>
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
#include "files.h"
#include "message.h"
#include "pilha.h"
#include "tokenlizer.h"

#define BUFFER_SIZE (64 * 1024 * 1024)

#ifndef NETINTERFACE
#error NETINTERFACE não está definido. Use "make IF=<interface de rede>"
//   definição do macro só pro editor de texto saber que é um macro
#define NETINTERFACE "ERROR"
#endif

type_pilha *freeHeap;

typedef enum ResultType {
    OK,
    EXIT,
    ERRO,
} ResultType;

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
    EXECUTANDO_REC,
    RECEBENDO_REC_FILE,
    ESPERANDO_MD5,  // Comando de verificação
    CD_CLIENT = 100,
    CD_SERVER
};

typedef struct Client {
    int socket;
    unsigned char *buffer;
    FILE *curr_file;
    char **argv;
    int argc;
} Client;

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
                comando = C_BACKUP_FILE;
            else if (argc > 2)
                comando = C_BACKUP_GROUP;
            else
                comando = -1;
            break;

        case REC:
            if (argc == 2)
                comando = C_RECOVER_FILE;
            else if (argc > 2)
                comando = C_RECOVER_GROUP;
            else
                comando = -1;
            break;

        case CDSV:
            // Deve conter o comando e apenas um diretório
            if (argc == 2)
                comando = C_CD_SERVER;
            else
                comando = -1;
            break;

        case VERIFICAR:
            // Deve conter o comando e apenas um arquivo
            if (argc == 2)
                comando = C_VERIFY;
            else
                comando = -1;
            break;

        case CD:
            comando = CD_CLIENT;
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

int interpreta_comando(int comando) {
    switch (comando) {
        case C_BACKUP_FILE:
        case C_BACKUP_GROUP:
            return EXECUTANDO_BACKUP;

        case C_RECOVER_FILE:
        case C_RECOVER_GROUP:
            return EXECUTANDO_REC;

        case C_CD_SERVER:
            return CD_SERVER;

        case C_VERIFY:
            return ESPERANDO_MD5;

        case CD_CLIENT:
            return CD_CLIENT;

        default:
            return ERRO;
    }  // switch (comando)
}

int le_comando(int *argc, char *argv[]) {
    char buf[4096];
    size_t len;

    fprintf(stdout, "%s >>> ", getcwd(buf, 4096));
    fgets(buf, 4096, stdin);
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[--len] = '\0';
    if (tokenlize(buf, argc, argv)) {
        exit(-1);
    }

    return identifica_comando(argv, *argc);
}

int executando_backup(Client *client) {
    unsigned char type;
    char error = 0;
    int r = OK;
    message_t messageR;
    int arquivos_processados = 0;
    int qnt_arquivos = client->argc - 1;
    for (int i = 1; i < client->argc; i++) {
        if (access(client->argv[i], R_OK) != 0) {
            printf("  \"%s\" \033[1;31m(Arquivo inexistante ou sem permissão de leitura)\033[0m\n", client->argv[i]);
            r = ERRO;
        } else if (!is_regular_file(client->argv[i])) {
            printf("  \"%s\" \033[1;31m(Arquivo não é um arquivo regular)\033[0m\n", client->argv[i]);
            r = ERRO;
        } else {
            printf("  \"%s\"\n", client->argv[i]);
        }
    }
    // No caso de um erro ter ocorrido:
    if (r == ERRO) {
        printf("\033[1;31mOperação abortada.\033[0m\n");
        return OK;
    }

    while (arquivos_processados < qnt_arquivos) {
        char *filename = client->argv[arquivos_processados + 1];
        r = ERRO;
        if (open_file(&(client->curr_file), filename) == 0)
            if (cm_send_message(client->socket, filename, strlen(filename) + sizeof((char)'\0'), C_BACKUP_FILE, NULL) != -1)
                if (cm_receive_message(client->socket, &error, 1, &type) != -1)
                    if (type == C_OK)
                        r = OK;

        if (r != OK)
            return r;

        while (!feof(client->curr_file)) {
            int bytes = fread(client->buffer, 1, BUFFER_SIZE, client->curr_file);
            int offset = 0;
            int sent;
            while (bytes != 0) {
                if ((sent = cm_send_message(client->socket, client->buffer + offset, bytes, C_DATA, &messageR)) == -1) {
                    messageError *messageErr = (messageError *)&messageR;
                    if (messageErr->type == C_ERROR && messageErr->errorCode == BUFFER_FULL) {
                        offset += messageErr->extraInfo;
                        sent -= messageErr->extraInfo;
                    } else {
                        exit(-1);
                    }
                }

                bytes -= sent;
            }
        }

        // Chegou no final do arquivo
        arquivos_processados += 1;
        r = ERRO;
        if (cm_send_message(client->socket, client->buffer, 0, C_END_OF_FILE, NULL) != -1)
            if (cm_receive_message(client->socket, &error, 1, &type) != -1)
                if (type == C_OK) {  // Enviou todo o arquivo.
                    r = OK;
                    fprintf(stdout, "\033[%dA\033[0K\r", qnt_arquivos - arquivos_processados + 1);
                    fprintf(stdout, "  \"%s\" \033[0;32m(Enviado)", client->argv[arquivos_processados]);
                    fprintf(stdout, "\033[%dB\033[0K\r\033[0m", qnt_arquivos - arquivos_processados + 1);
                    fflush(stdout);
                }

        fclose(client->curr_file);
        client->curr_file = NULL;

        if (r == ERRO) {
            fprintf(stdout, "\033[%dA\033[0K\r", qnt_arquivos - arquivos_processados + 1);
            fprintf(stdout, "  \"%s\" \033[1;31m(ERRO)", client->argv[arquivos_processados]);
            fprintf(stdout, "\033[%dB\033[0K\r\033[0m", qnt_arquivos - arquivos_processados + 1);
            fflush(stdout);
            return r;
        }
    }  // while(arquivos_processados < qnt_arquivos)

    return OK;
}

int executando_rec(Client *client) {
    int r = ERRO;
    unsigned char type;
    int bytes;
    message_t messageR;
    int arquivos_processados = 0;
    int qnt_arquivos;
    // Verifica se os arquivos existem no servidor
    // Ou se algum erro de leitura ocorre
    client->buffer[0] = (unsigned char)'\0';
    for (int i = 1; i < client->argc; i++) {
        strcat((char *)client->buffer, " ");
        strcat((char *)client->buffer, client->argv[i]);
    }
    if (cm_send_message(client->socket, client->buffer, strlen((char *)client->buffer) + sizeof((char)'\0'), C_RECOVER_FILE, &messageR) != -1) {
        if (cm_receive_message(client->socket, client->buffer, sizeof(int), &type) != -1) {
            if (type == C_OK) {
                qnt_arquivos = (int)(client->buffer[0]);
                r = OK;
            }
        } else {
            if (type == C_ERROR) {
                if (((messageError *)client->buffer)->errorCode == NO_READ_PERMISSION)
                    fprintf(stderr, "O servidor não tem permissão de leitura de um dos arquivos requesitados.\n");
                else if (((messageError *)client->buffer)->errorCode == FILE_NOT_FOUND)
                    fprintf(stderr, "Um ou mais dos arquivos requisitados não existem.\n");
                else if (((messageError *)client->buffer)->errorCode == CHECK_ERRNO) {
                    errno = ((messageError *)client->buffer)->errnoCode;
                    fprintf(stderr, "Erro no servidor: %s.\n", strerror(errno));
                }

                return OK;
            }
        }
    }

    while (arquivos_processados < qnt_arquivos) {
        // Cliente está pronto para receber um arquivo
        if (cm_send_message(client->socket, client->buffer, 0, C_OK, &messageR) == -1)
            exit(-1);

        // Recebe o nome de um arquivo
        if ((bytes = cm_receive_message(client->socket, client->buffer, BUFFER_SIZE, &type)) == -1)
            exit(-1);
        if (type != C_FILE_NAME) {
            fprintf(stderr, "Cliente recebeu pacote do tipo errado. Esperado: C_FILE_NAME\n");
            exit(-1);
        }
        char *filename = (char *)client->buffer;

        // Abre esse arquivo
        // e manda uma resposta confirmando
        printf("Iniciando a recuperação do arquivo \"%s\".\n", filename);
        client->curr_file = fopen(filename, "w+");
        if (client->curr_file == NULL) {
            printf("Falha ao abrir o arquivo. Recuperação abortada.\n");
            r = ERRO;
            type = C_ERROR;
            bytes = 2;
            ((messageError *)client->buffer)->errorCode = CHECK_ERRNO;
            ((messageError *)client->buffer)->errnoCode = errno;
        } else {
            r = OK;
            type = C_OK;
            bytes = 0;
        }
        if (cm_send_message(client->socket, client->buffer, bytes, type, &messageR) == -1)
            exit(-1);

        if (r != OK)
            return r;

        for (;;) {
            if ((bytes = cm_receive_message(client->socket, client->buffer, BUFFER_SIZE, &type)) == -1)
                exit(-1);

            if (type == C_DATA) {
                fwrite(client->buffer, 1, bytes, client->curr_file);
            } else if (type == C_END_OF_FILE) {
                fclose(client->curr_file);
                client->curr_file = NULL;
                if (cm_send_message(client->socket, client->buffer, 0, C_OK, &messageR) == -1)
                    exit(-1);
                printf("Recuperação Concluida.\n");
                arquivos_processados += 1;
                break;
            } else {
                return ERRO;
            }
        }
    }

    return OK;
}

int cd_server(Client *client) {
    // Envia o caminho do diretório para o server.
    int r = ERRO;
    unsigned char type;
    message_t messageR;
    if (cm_send_message(client->socket, client->argv[client->argc - 1], strlen(client->argv[client->argc - 1]) + sizeof((char)'\0'), C_CD_SERVER, &messageR) != -1)
        if (cm_receive_message(client->socket, client->buffer, 2, &type) != -1) {
            if (type == C_OK) {
                printf("Diretório alterado para: \"%s\"\n", client->argv[client->argc - 1]);
                r = OK;
            } else if (type == C_ERROR && ((messageError *)client->buffer)->errorCode == CHECK_ERRNO)
                fprintf(stderr, "Ocorreu um erro no servidor.\n\t%s\n", strerror(((messageError *)client->buffer)->errnoCode));
        }

    return r;
}

int verificar_md5(Client *client) {
    printf("Verificando o arquivo: %s\n", client->argv[client->argc - 1]);
    MD5_CTX md5;
    unsigned char hash_result_client[MD5_DIGEST_LENGTH];
    int r;
    int bytes;
    unsigned char type;
    message_t messageR;

    // Calcula o md5 do lado do client
    MD5_Init(&md5);
    r = ERRO;
    if ((errno = open_file(&client->curr_file, client->argv[client->argc - 1])) != 0) {
        return ERRO;
    }
    while (!feof(client->curr_file)) {
        bytes = fread(client->buffer, 1, BUFFER_SIZE, client->curr_file);
        MD5_Update(&md5, client->buffer, bytes);
    }
    MD5_Final(hash_result_client, &md5);

    // Envia para o servidor e espera o md5 do lado do servidor
    if (cm_send_message(client->socket, client->argv[client->argc - 1], strlen(client->argv[client->argc - 1]) + sizeof((char)'\0'), C_VERIFY, &messageR) != -1)
        if (cm_receive_message(client->socket, client->buffer, MD5_DIGEST_LENGTH, &type) != -1) {
            r = OK;
            if (type == C_MD5) {
                if (memcmp(client->buffer, hash_result_client, MD5_DIGEST_LENGTH) == 0) {
                    printf("Os arquivos são idênticos.\n");
                } else {
                    printf("Esse arquivo está diferente do guardado no backup.\n");
                }
            } else if (type == C_ERROR) {
                if (((messageError *)client->buffer)->errorCode == FILE_NOT_FOUND) {
                    printf("Arquivo não foi encontrado no servidor.\n");
                } else if (((messageError *)client->buffer)->errorCode == NO_READ_PERMISSION) {
                    printf("O servidor não tem permissão de leitura sobre o arquivo.\n");
                } else if (((messageError *)client->buffer)->errorCode == CHECK_ERRNO) {
                    printf("Ocorreu um erro no servidor: %s\n", strerror(((messageError *)client->buffer)->errnoCode));
                }
            }
        }

    return r;
}
int cd_client(Client *client) {
    printf("Mudando para o diretório: %s\n", client->argv[client->argc - 1]);
    if (chdir(client->argv[client->argc - 1]))
        return ERRO;
    else
        return OK;
}

int command_handler(Client *client, int comando) {
    switch (comando) {
        case C_BACKUP_FILE:
        case C_BACKUP_GROUP:
            return executando_backup(client);

        case C_RECOVER_FILE:
        case C_RECOVER_GROUP:
            return executando_rec(client);

        case C_CD_SERVER:
            return cd_server(client);

        case C_VERIFY:
            return verificar_md5(client);

        case CD_CLIENT:
            return cd_client(client);

        default:
            return ERRO;
    }  // switch (comando)
}

int main(void) {
    freeHeap = criar_pilha(sizeof(void *));
    on_exit(libera_e_sai, freeHeap);

    Client client;

    client.socket = ConexaoRawSocket(NETINTERFACE);

    char *argv[4096];
    client.argv = argv;
    client.curr_file = NULL;
    client.buffer = malloc(BUFFER_SIZE * sizeof(unsigned char));
    empilhar(freeHeap, client.buffer);
    if (client.buffer == NULL) {
        printf("Falha ao alocar buffer de arquivo.\n");
        exit(-1);
    }

    printModoDeUso();
    for (;;) {
        int comando = le_comando(&(client.argc), client.argv);
        if (comando == -1) {
            printModoDeUso();
            continue;
        }

        int r = command_handler(&client, comando);
        switch (r) {
            case OK:
                // Code
                break;

            case ERRO:
                fprintf(stderr, "Ocorreu um erro.\n\t%s\n", strerror(errno));
                break;

            case EXIT:
                exit(0);
        }
    }

    exit(0);
}