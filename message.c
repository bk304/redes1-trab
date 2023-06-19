#include "message.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "ethernet.h"
#include "pthread.h"

#define MESSAGE_PROTOCOL_ID (htons(0x7304))

pthread_mutex_t *message_mutex = NULL;
unsigned char selfIdentifier = 0;
#ifdef LO
unsigned char lastParity = 0xFF;
unsigned char lastSeq = 90;
#endif

void *packetPtr_from_message(t_message *message) {
    return ((void *)message) - 14;
}

t_message *init_message(void *packet_buffer) {
#ifdef DEBUG
    if (message_mutex == NULL) {
        message_mutex = malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(message_mutex, NULL);
    }
#endif
    if (selfIdentifier == 0)
        selfIdentifier = (unsigned char)time(NULL);
    t_ethernet_frame *ethernet_packet = (t_ethernet_frame *)packet_buffer;
    memset(ethernet_packet->mac_destination, 0x00, 6);
    memset(ethernet_packet->mac_source, 0x00, 6);
    ethernet_packet->mac_source[0] = selfIdentifier;
    *((short *)ethernet_packet->len_or_type) = MESSAGE_PROTOCOL_ID;
    t_message *message = (t_message *)ethernet_packet->payload;

    message->start_frame_delimiter = START_FRAME_DELIMITER;

    return message;
}

char *message_type_str(unsigned char type_code) {
    switch (type_code) {
        case C_BACKUP_1FILE:
            return "backup de 1 arquivo";
        case C_BACKUP_GROUP:
            return "backup de grupo de arquivos";
        case C_RECOVER_1FILE:
            return "recuperar 1 arquivo";
        case C_RECOVER_GROUP:
            return "recuperar um grupo de arquivos";
        case C_CD_SERVER:
            return "troca de diretório";
        case C_VERIFY:
            return "verificar integridade de arquivo";
        case C_FILE_NAME:
            return "arquivo com nome";
        case C_MD5:
            return "COMANDO INEXISTENTE";
        case C_DATA:
            return "dado de arquivo";
        case C_END_OF_FILE:
            return "fim de arquivo";
        case C_END_OF_GROUP:
            return "fim do grupo de arquivos";
        case C_METAMESSAGE:
            return "Meta Mensagem";
        case C_ERROR:
            return "erro";
        case C_OK:
            return "ok";
        case C_ACK:
            return "ack";
        case C_NACK:
            return "nack";
    }

    return NULL;
}

int send_message(int socket, t_message *message) {
    message->parity = message_parity(message);
    t_ethernet_frame *ethernet_packet = packetPtr_from_message(message);
    ethernet_packet->mac_source[0] = selfIdentifier;
#ifdef DEBUG1
    int old_state;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
    pthread_mutex_lock(message_mutex);
    printf("\033[0;32m");
    printf("Enviando ");
    printMessage(message);
    prinfhexMessage(message);
    printf("\033[0m");
    printf("\n");
    pthread_mutex_unlock(message_mutex);
    pthread_setcancelstate(old_state, &old_state);
#endif
    return send(socket, packetPtr_from_message(message), PACKET_SIZE_BYTES, 0);
}

int send_nack(int socket, t_message *messageA, t_message *messageR) {
    messageA->length = 0;
    messageA->sequence = messageR->sequence;
    messageA->type = C_NACK;
    return send_message(socket, messageA);
}

int receive_message(int socket, t_message *message) {
    int read_status;
    void *packet = packetPtr_from_message(message);
    t_ethernet_frame *ethernet_packet = (t_ethernet_frame *)packet;

    for (;;) {
        read_status = recv(socket, packet, PACKET_SIZE_BYTES, MSG_TRUNC);

        if (read_status == -1) {
            return RECVM_STATUS_ERROR;
        }

        if (read_status != PACKET_SIZE_BYTES) {
            continue;
        }

        if (*(short *)(ethernet_packet->len_or_type) != MESSAGE_PROTOCOL_ID) {
            continue;
        }

        if (message->start_frame_delimiter != START_FRAME_DELIMITER) {
            continue;
        }

        if (ethernet_packet->mac_source[0] == selfIdentifier) {
            continue;
        }

#ifdef LO
        if (message->parity == lastParity && message->sequence == lastSeq) {
#ifdef SHOWDELETED
            int old_state;
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
            pthread_mutex_lock(message_mutex);
            printf("\033[1;31m");
            printf("MENSAGEM BLOQUEADA ");
            printMessage(message);
            prinfhexMessage(message);
            printf("\033[0m");
            printf("\n");
            pthread_mutex_unlock(message_mutex);
            pthread_setcancelstate(old_state, &old_state);
#endif
            continue;
        }
#endif

        break;
    }

#ifdef DEBUG
    int old_state;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
    pthread_mutex_lock(message_mutex);
    printf("\033[1;33m");
    printf("Recebido ");
    printMessage(message);
    prinfhexMessage(message);
    printf("\033[0m");
    printf("\n");
    pthread_mutex_unlock(message_mutex);
    pthread_setcancelstate(old_state, &old_state);
#endif

#ifdef LO
    lastParity = message->parity;
    lastSeq = message->sequence;
#endif

    if (message->parity != message_parity(message)) {
        return RECVM_STATUS_PARITY_ERROR;
    }

    return read_status;
}

unsigned char message_parity(t_message *message) {
    unsigned char paridade = 0b00000000;

    // A paridade só será feita nos campos
    // Tamanho, Seq, Tipo E Dados.
    // Por isso estou somando 1, para pular o campo de marcador de inicio.
    unsigned char *message_byByte = ((void *)message) + 1;

    for (int i = 0; i < (DATA_MAX_SIZE_BYTES + 2); i++) {
        paridade ^= message_byByte[i];
    }

    return paridade % 255;
}

void printMessage(t_message *message) {
    printf("SEQ: %d | TYPE: %s | TAMANHO: %d | PARITY: %d", message->sequence, message_type_str(message->type), message->length, message->parity);
}

void prinfhexMessage(t_message *message) {
    printf("> \n");
    for (int i = 0; i < MESSAGE_SIZE_BYTES; i++) {
        printf("%02X ", ((unsigned char *)message)[i]);
    }
    printf(" <\n");
}

void flush_recv_queue(int socket) {
    int save = errno;
    unsigned char *trash = malloc(PACKET_SIZE_BYTES * sizeof(unsigned char));
    t_message *messageR = init_message(trash);
#ifdef DEBUG
    int old_state;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);
    pthread_mutex_lock(message_mutex);
    printf("Dando flush\n");
    pthread_mutex_unlock(message_mutex);
#endif
    for (;;) {
        if (!(recv(socket, trash, PACKET_SIZE_BYTES, MSG_DONTWAIT | MSG_PEEK) > 0))
            break;

        if (messageR->type == C_METAMESSAGE && messageR->data[0] > 1)
            break;

        recv(socket, trash, PACKET_SIZE_BYTES, MSG_DONTWAIT);
    }
#ifdef DEBUG
    pthread_mutex_lock(message_mutex);
    printf("Pronto. Flush feito.\n");
    pthread_mutex_unlock(message_mutex);
    pthread_setcancelstate(old_state, &old_state);
#endif

    free(trash);
    errno = save;
}