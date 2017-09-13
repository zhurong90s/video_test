#ifndef PACKETS_H
#define PACKETS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>


struct v_msg{

    void * handle_packet;
};

struct packets_head{
    uint16_t len;
    uint16_t type;
    uint16_t seq;
    uint16_t dist;
    uint8_t *data[0];
}__attribute__((packed));

struct attribute{
    uint16_t type;
    uint16_t len;
}__attribute__((packed));

#endif // PACKETS_H
