#include <string.h>

#include "message.h"

void set_start_delimiter(t_message *message){
    message->start_frame_delimiter = START_FRAME_DELIMITER;
}

void set_message(t_message *message, int length, int seq, int type, t_message_data *data){
    message->length = length;
    message->sequence = seq;
    message->type = type;
    memcpy(&message->data, data, DATA_MAX_SIZE_BYTES);
}