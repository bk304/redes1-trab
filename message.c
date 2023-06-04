#include "message.h"

#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "ethernet.h"

#define MESSAGE_PROTOCOL_ID (htons(0x7304))

void *packetPtr_from_message(t_message *message) {
    return ((void *)message) - 14;
}

t_message *init_message(void *packet_buffer) {
    t_ethernet_frame *ethernet_packet = (t_ethernet_frame *)packet_buffer;
    memset(ethernet_packet->mac_destination, 0x00, 6);
    memset(ethernet_packet->mac_source, 0x00, 6);
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
        case C_UNUSED_1:
            return "COMANDO INEXISTENTE";
        case C_DATA:
            return "dado de arquivo";
        case C_END_OF_FILE:
            return "fim de arquivo";
        case C_END_OF_GROUP:
            return "fim do grupo de arquivos";
        case C_UNUSED_2:
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

int send_message(int socket, t_message *message, int seq, int type, void *data, int length) {
    if (length < 0 || length > DATA_MAX_SIZE_BYTES)
        return -1;

    message->length = length;
    message->sequence = seq;
    message->type = type;
    (void)memcpy(message->data, data, length);

    return send(socket, packetPtr_from_message(message), PACKET_SIZE_BYTES, 0);
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

        break;
    }

    return read_status;
}