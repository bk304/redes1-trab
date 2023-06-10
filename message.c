#include "message.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "ethernet.h"

#define MESSAGE_PROTOCOL_ID (htons(0x7304))

unsigned char selfCode = 0;
unsigned char lastSeq = 90;

void *packetPtr_from_message(t_message *message) {
    return ((void *)message) - 14;
}

t_message *init_message(void *packet_buffer) {
    if (selfCode == 0)
        selfCode = (unsigned char)time(NULL);
    t_ethernet_frame *ethernet_packet = (t_ethernet_frame *)packet_buffer;
    memset(ethernet_packet->mac_destination, 0x00, 6);
    memset(ethernet_packet->mac_source, 0x00, 6);
    ethernet_packet->mac_source[0] = selfCode;
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
        case C_UNUSED:
            return "COMANDO INEXISTENTE";
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
    ethernet_packet->mac_source[0] = selfCode;
#ifdef DEBUG
    printf("Enviando ");
    printMessage(message);
    prinfhexMessage(message);
    printf("\n");
#endif
    return send(socket, packetPtr_from_message(message), PACKET_SIZE_BYTES, 0);
}

int send_ack(int socket, t_message *message) {
    message->length = 0;
    message->sequence = 0;
    message->type = C_ACK;
    return send_message(socket, message);
}

int send_nack(int socket, t_message *message) {
    message->length = 0;
    message->sequence = 0;
    message->type = C_NACK;
    return send_message(socket, message);
}

int receive_message(int socket, t_message *message) {
    int read_status;
    void *packet = packetPtr_from_message(message);
    t_ethernet_frame *ethernet_packet = (t_ethernet_frame *)packet;

    for (;;) {
        read_status = recv(socket, packet, PACKET_SIZE_BYTES, MSG_TRUNC);

        if (read_status == -1) {
            return read_status;
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

        if (ethernet_packet->mac_source[0] == selfCode) {
            continue;
        }

        if (message->sequence == lastSeq) {
            continue;
        }

        break;
    }

#ifdef DEBUG
    printf("Recebido ");
    printMessage(message);
    prinfhexMessage(message);
    printf("\n");
#endif

    lastSeq = message->sequence;

    if (message->parity != message_parity(message)) {
        read_status = -2;
        return read_status;
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
        printf("%02X(%d) ", ((unsigned char *)message)[i], i);
    }
    printf(" <\n");
}
