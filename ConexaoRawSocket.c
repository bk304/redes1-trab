#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

int ConexaoRawSocket(char *device) {
    int soquete;
    struct ifreq ir;
    struct sockaddr_ll endereco;
    struct packet_mreq mr;

    soquete = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL)); /*cria socket*/
    if (soquete == -1) {
        printf("Erro no Socket\n Você é root?\n");
        exit(-1);
    }

    memset(&ir, 0, sizeof(struct ifreq)); /*dispositivo eth0*/
    // memcpy(ir.ifr_name, device, sizeof(device));
    memcpy(ir.ifr_name, device, strlen(device));
    if (ioctl(soquete, SIOCGIFINDEX, &ir) == -1) {
        printf("Erro no ioctl\n");
        exit(-1);
    }

    memset(&endereco, 0, sizeof(endereco)); /*IP do dispositivo*/
    endereco.sll_family = AF_PACKET;
    endereco.sll_ifindex = ir.ifr_ifindex;
    endereco.sll_protocol = htons(ETH_P_ALL);
    if (bind(soquete, (struct sockaddr *)&endereco, sizeof(endereco)) == -1) {
        printf("Erro no bind\n");
        exit(-1);
    }

    memset(&mr, 0, sizeof(mr)); /*Modo Promiscuo*/
    mr.mr_ifindex = ir.ifr_ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    if (setsockopt(soquete, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1) {
        printf("Erro ao fazer setsockopt\n");
        exit(-1);
    }

    return soquete;
}
