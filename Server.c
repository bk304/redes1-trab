#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <openssl/md5.h>
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

#ifndef NETINTERFACE
#error "NETINTERFACE não está definido. Use 'make [interface de rede]"
// define só para o editor de texto saber que é um macro.
#define NETINTERFACE "lo"
#endif

#define BUFFER_SIZE (64 * 1024 * 1024)

#define SV_ROOT_PATH ("./serverFolder")
#define DEFAULT_PATH ("./serverFolder/backups/default")

typedef enum ResultType {
    OK,
    ERRO_NO_CLIENT_WARNING,
    ERRO,
} ResultType;

typedef struct Result {
    ResultType type;
    int v;
} Result;

enum REQUEST_HANDLER_ERRORS {
    BROKEN_LOGIC,
};

typedef struct Server {
    int socket;
    unsigned char *buffer;
    FILE *curr_file;
} Server;

Result receber_backup_file(Server *server);
Result receber_backup_group(Server *server);
Result enviar_recover_file(Server *server);
Result enviar_recover_group(Server *server);
Result cd_server(Server *server);
Result verifica_backup(Server *server);

Result receber_backup_file(Server *server) {
    Result r;
    unsigned char typeT, typeR;
    int bytesT, bytesR;
    message_t messageR;

    printf("Iniciando backup do arquivo \"%s\".\n", server->buffer);
    server->curr_file = fopen(getFileName((char *)server->buffer), "w+");
    if (server->curr_file == NULL) {
        printf("Falha ao abrir o arquivo. Backup abortado.\n");
        r.type = ERRO;
        typeT = C_ERROR;
        bytesT = 1;
        if (errno == EACCES)
            ((messageError *)server->buffer)->errorCode = NO_WRITE_PERMISSION;
        else if (errno == EDQUOT)
            ((messageError *)server->buffer)->errorCode = DISK_FULL;
        else {
            bytesT = 2;
            ((messageError *)server->buffer)->errorCode = CHECK_ERRNO;
            ((messageError *)server->buffer)->errnoCode = errno;
        }
    } else {
        typeT = C_OK;
        bytesT = 0;
    }
    if (cm_send_message(server->socket, server->buffer, bytesT, typeT, &messageR) == -1)
        exit(-1);

    if (messageR.type == C_ERROR) {
        r.type = ERRO;
        return r;
    }

    for (;;) {
        if ((bytesR = cm_receive_message(server->socket, server->buffer, BUFFER_SIZE, &typeR)) == -1)
            exit(-1);

        if (typeR == C_DATA) {
            fwrite(server->buffer, 1, bytesR, server->curr_file);
        } else if (typeR == C_END_OF_FILE) {
            fclose(server->curr_file);
            if (cm_send_message(server->socket, server->buffer, 0, C_OK, &messageR) == -1)
                exit(-1);
            if (messageR.type == C_ERROR) {
                r.type = ERRO;
                return r;
            }
            break;
        } else {
            printf("BROKEN LOGIC: %d\n", typeR);
            r.type = ERRO;
            return r;
        }
    }

    printf("Backup Concluido.\n");
    r.type = OK;
    return r;
}

Result receber_backup_group(Server *server) {
    Result r;
    r.type = ERRO;
    return r;
}

Result enviar_recover_file(Server *server) {
    Result r;
    unsigned char typeT, typeR;
    int bytesT, bytesR;
    message_t messageR;
    int argc;
    char *argv[4096];
    int arquivos_processados = 0;
    int qnt_arquivos = 0;
    int bytesToSend;
    int offset = 0;
    int status;

    // Separa nomes com asterisco (*) em todas as possibilidades
    if (tokenlize((char *)server->buffer, &argc, argv)) {
        exit(-1);
    }

    // Verifica se todos os arquivos são acessiveis
    r.type = OK;
    for (int i = 0; i < argc; i++) {
        if (access(argv[i], R_OK) != 0) {
            printf("  \"%s\" \033[1;31m(Arquivo inexistante ou sem permissão de leitura)\033[0m\n", argv[i]);
            r.type = ERRO;
        } else {
            printf("  \"%s\"\n", argv[i]);
        }
    }

    if (r.type != ERRO) {
        // Se tudo foi OK:
        typeT = C_OK;
        bytesT = sizeof(int);
        qnt_arquivos = argc;
        ((int *)server->buffer)[0] = qnt_arquivos;
    } else {
        // No caso de um erro ter ocorrido:
        printf("\033[1;31mOperação abortada.\033[0m\n");
        typeT = C_ERROR;
        bytesT = 1;
        if (errno == EACCES)
            ((messageError *)server->buffer)->errorCode = NO_READ_PERMISSION;
        else if (errno == ENOENT)
            ((messageError *)server->buffer)->errorCode = FILE_NOT_FOUND;
        else {
            bytesT = 2;
            ((messageError *)server->buffer)->errorCode = CHECK_ERRNO;
            ((messageError *)server->buffer)->errnoCode = errno;
        }
    }

    if (cm_send_message(server->socket, server->buffer, bytesT, typeT, &messageR) == -1)
        exit(-1);
    if (messageR.type == C_ERROR) {
        r.type = ERRO;
        return r;
    }

    if (r.type == ERRO) {
        // A mensagem que avisa o client já foi enviada
        // Então podemos sair da função;
        return r;
    }

    if (cm_receive_message(server->socket, server->buffer, BUFFER_SIZE, (unsigned char *)&typeR) == -1)
        exit(-1);

    // Já que são acessiveis, agora começará a enviar eles
    while (arquivos_processados < qnt_arquivos) {
        //
        // Abre um arquivo e envia o nome para o cliente.
        // Caso o cliente consiga criar um arquivo, continua.
        char *filename = argv[arquivos_processados];
        r.type = ERRO;
        if (open_file(&(server->curr_file), filename) == 0)
            if (cm_send_message(server->socket, filename, strlen(filename) + sizeof((char)'\0'), C_FILE_NAME, &messageR) != -1)
                if (cm_receive_message(server->socket, server->buffer, BUFFER_SIZE, (unsigned char *)&typeR) != -1) {
                    if (typeR == C_OK)
                        r.type = OK;
                    else {
                        r.type = ERRO_NO_CLIENT_WARNING;
                        errno = ((messageError *)server->buffer)->errnoCode;
                    }
                }

        if (r.type != OK) {
            printf("Broken Logic 3: %d\n", typeR);
            return r;
        }

        // O arquivo esta aberto
        // Aqui ele será enviado
        while (!feof(server->curr_file)) {
            bytesToSend = fread(server->buffer, 1, BUFFER_SIZE, server->curr_file);
            while (bytesToSend != 0) {
                status = cm_send_message(server->socket, server->buffer + offset, bytesToSend, C_DATA, &messageR);
                if (status == -1) {
                    messageError *messageErr = (messageError *)&messageR;
                    if (messageErr->type == C_ERROR && messageErr->errorCode == BUFFER_FULL) {
                        offset += messageErr->extraInfo;
                        bytesToSend -= messageErr->extraInfo;
                    } else {
                        exit(-1);
                    }
                }

                bytesToSend -= status;
            }
        }

        // Chegou no final do arquivo
        arquivos_processados += 1;
        r.type = ERRO;
        if (cm_send_message(server->socket, server->buffer, 0, C_END_OF_FILE, &messageR) != -1)
            if (cm_receive_message(server->socket, server->buffer, 1, &typeR) != -1)
                if (typeR == C_OK) {
                    r.type = OK;
                    fprintf(stdout, "\033[%dA\033[0K\r", qnt_arquivos - arquivos_processados + 1);
                    fprintf(stdout, "  \"%s\" \033[0;32m(Enviado)", argv[arquivos_processados - 1]);
                    fprintf(stdout, "\033[%dB\033[0K\r\033[0m", qnt_arquivos - arquivos_processados + 1);
                    fflush(stdout);
                }

        if (r.type != OK) {
            fprintf(stdout, "\033[%dA\033[0K\r", qnt_arquivos - arquivos_processados + 1);
            fprintf(stdout, "  \"%s\" \033[1;31m(ERRO)", argv[arquivos_processados - 1]);
            fprintf(stdout, "\033[%dB\033[0K\r\033[0m", qnt_arquivos - arquivos_processados + 1);
            fflush(stdout);
            return r;
        }
    }  // while (arquivos_processados < qnt_arquivos)

    r.type = OK;
    return r;
}

Result enviar_recover_group(Server *server) {
    Result r;
    r.type = ERRO;
    return r;
}

Result cd_server(Server *server) {
    Result r;
    unsigned char typeT, typeR;
    int bytesT, bytesR;
    message_t messageR;

    printf("Trocando de diretório para: \"%s\".\n", server->buffer);
    mkdir_p((char *)server->buffer, S_IRUSR | S_IWUSR);
    if (chdir((char *)server->buffer) != 0) {
        printf("Impossível trocar para esse diretório.\n");
        r.type = ERRO;
        typeT = C_ERROR;
        bytesT = 2;
        ((messageError *)server->buffer)->errorCode = CHECK_ERRNO;
        ((messageError *)server->buffer)->errnoCode = errno;
    } else {
        typeT = C_OK;
        bytesT = 0;
    }
    if (cm_send_message(server->socket, server->buffer, bytesT, typeT, &messageR) == -1)
        exit(-1);

    r.type = OK;
    return r;
}

Result verifica_backup(Server *server) {
    Result r;
    r.type = ERRO;
    return r;
}

Result request_handler(Server *server, int bytesReceived, unsigned int typeReceived) {
    Result r;
    Result status;

    if (!(isCommandMessageType(typeReceived))) {
        r.type = ERRO;
        r.v = BROKEN_LOGIC;  // O Client deve ser reiniciado.
        return r;
    }
    // São as mensagens dos tipos:
    //      C_BACKUP_FILE 0b0000
    //      C_BACKUP_GROUP 0b0001
    //      C_RECOVER_FILE 0b0010
    //      C_RECOVER_GROUP 0b0011
    //      C_CD_SERVER 0b0100
    //      C_VERIFY 0b0101

    switch (typeReceived) {
        case C_BACKUP_FILE:
            r = receber_backup_file(server);
            break;

        case C_BACKUP_GROUP:
            r = receber_backup_group(server);
            break;

        case C_RECOVER_FILE:
            r = enviar_recover_file(server);
            break;

        case C_RECOVER_GROUP:
            r = enviar_recover_group(server);
            break;

        case C_CD_SERVER:
            r = cd_server(server);
            break;

        case C_VERIFY:
            r = verifica_backup(server);
            break;

        default:
            exit(-1);
    }
}

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

    char *argStr = malloc(2 * ARG_MAX_SIZE * sizeof(char));
    empilhar(freeHeap, argStr);

    void *packets_buffer = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
    empilhar(freeHeap, packets_buffer);
    message_t *messageR = init_message(packets_buffer);  // Você recebe uma resposta. (Pacote recebido)

    Server server;
    server.socket = ConexaoRawSocket(NETINTERFACE);
    server.curr_file = NULL;
    server.buffer = malloc(BUFFER_SIZE * sizeof(unsigned char));
    empilhar(freeHeap, server.buffer);
    if (server.buffer == NULL) {
        printf("Falha ao alocar buffer do server.\n");
        exit(-1);
    }

    Result r;
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

    // NEW
    for (;;) {
        if ((bytesR = cm_receive_message(server.socket, server.buffer, BUFFER_SIZE, &typeR)) == -1)
            exit(-1);

        r = request_handler(&server, bytesR, typeR);
        switch (r.type) {
            case OK:
                /* code */
                break;

            case ERRO:
                // Envia mensagem pro client pedindo para reiniciar.
                // E volta a ouvir por comandos
                printf("Ocorreu um erro.\n\t%s\n", strerror(errno));
                break;

            case ERRO_NO_CLIENT_WARNING:
                // Faz a mesma coisa que o ERRO normal, mas não vai enviar uma mensagem avisando o client.
                printf("Ocorreu um erro.\n\t%s\n", strerror(errno));
                break;
        }
    }

    exit(0);
}