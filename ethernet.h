#ifndef __ETHERNET_H__
#define __ETHERNET_H__

typedef struct t_ethernet_frame {
    unsigned char mac_destination[6];
    unsigned char mac_source[6];
    unsigned char len_or_type[2];
    unsigned char payload[0];
} t_ethernet_frame;

#endif  // __ETHERNET_H__