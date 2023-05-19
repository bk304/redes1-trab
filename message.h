#if !defined(__MESSAGE_H__)
#define __MESSAGE_H__

#define MESSAGE_SIZE 67                             // 67 bytes.
#define START_FRAME_DELIMITER_BITFIELD_SIZE 8       // 8 bits.
#define LENGTH_BITFIELD_SIZE 6                      // 6 bits.
#define SEQUENCE_BITFIELD_SIZE 6                    // 6 bits.
#define TYPE_BITFIELD_SIZE 4                        // 4 bits.
#define DATA_MAX_SIZE 504                           // 504 bits. 63 bytes.
#define PARITY_BITFIELD_SIZE 8                      // 8 bits.

typedef struct t_message {
    start_frame_delimiter: START_FRAME_DELIMITER_BITFIELD_SIZE;
    length: LENGTH_BITFIELD_SIZE;
    sequence: SEQUENCE_BITFIELD_SIZE;
    type: TYPE_BITFIELD_SIZE;
    data: DATA_MAX_SIZE;
    parity: PARITY_BITFIELD_SIZE;
} t_message;

typedef struct t_message_data {
    char data[63];
} t_message_data;

#define START_FRAME_DELIMITER 0b01111110


// MESSAGE CODES

#define C_BACKUP_1FILE  0b0000
#define C_BACKUP_GROUP  0b0001
#define C_RECOVER_1FILE 0b0010
#define C_RECOVER_GROUP 0b0011
#define C_CD_SERVER     0b0100
#define C_VERIFY        0b0101
#define C_FILE_NAME     0b0110
#define C_UNUSED_1      0b0111
#define C_DATA          0b1000
#define C_END_OF_FILE   0b1001
#define C_END_OF_GROUP  0b1010
#define C_UNUSED_2      0b1011
#define C_ERROR         0b110
#define C_OK            0b1101
#define C_ACK           0b1110
#define C_NACK          0b1111


// ERROR CODES

#define DISK_FULL 0
#define NO_WRITE_PERMISSION 1
#define FILE_NOT_FOUND 2
#define NO_READ_PERMISSION 3


void set_start_delimiter(t_message *message);
void set_message(int length, int seq, int type, t_message_data *data);

#endif // __MESSAGE_H__
