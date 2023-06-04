#include "message.h"

#include <string.h>

void set_start_delimiter(t_message *message) {
    message->start_frame_delimiter = START_FRAME_DELIMITER;
}

void set_message(t_message *message, int length, int seq, int type, void *data) {
    message->length = length;
    message->sequence = seq;
    message->type = type;
    memcpy(message->data, data, DATA_MAX_SIZE_BYTES);
}

char *message_type_str(unsigned char type_code) {
    switch (message->type) {
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
}