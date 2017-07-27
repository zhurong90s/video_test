#ifndef PACKET_H
#define PACKET_H

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

typedef uint32_t packet_lenght;
#define VIDEO_HEAD_TYPE_DATA        0x01
#define VIDEO_HEAD_TYPE_IDENTITY    0x02
#define VIDEO_HEAD_TYPE_REPLY       0x03
#define VIDEO_HEAD_TYPE_CTL       0x04

struct video_head{
    packet_lenght len;
    uint8_t type;
    uint8_t data[0];
}__attribute__((packed));

#define VIDEO_IDENTITY_KEY_SIZE 5

#define VIDEO_IDENTITY_WYA_VIEW 0x01
#define VIDEO_IDENTITY_WYA_COLLECT 0x02

struct video_identity{
    uint16_t id;
    uint8_t wya;
    uint8_t key[VIDEO_IDENTITY_KEY_SIZE];
}__attribute__((packed));

#define VIDEO_DATA_SEGMENT_LEN 1024
struct video_data{
    uint16_t seq;
    packet_lenght vdata_len;
    uint8_t vdata[0];
}__attribute__((packed));

#define VIDEO_CTL_CMD_PARAM_MAX 30
#define VIDEO_CTL_CMD_REQ_ONE_PACKETDAT 1
struct video_ctl{
    uint16_t seq;
    uint32_t cmd;
    packet_lenght param_len;
    uint8_t param[0];
}__attribute__((packed));

#define VIDEO_REPLY_STATUS_IDENTITY_OK  0x01
#define VIDEO_REPLY_STATUS_DATA_OK      0x02
#define VIDEO_REPLY_STATUS_DATA_ERR     0x03
#define VIDEO_REPLY_STATUS_CMD_OK     0x04
#define VIDEO_REPLY_STATUS_CMD_ERR     0x05
struct video_reply{
    uint8_t status;
    uint8_t reply2seq;
}__attribute__((packed));

struct video_packet_segment{
    uint16_t s_len;
    uint8_t *data;
};
struct video_packet{
    uint16_t p_len;
    uint8_t c_segment;
    struct video_packet_segment segment[0];
};

int create_server(int port);
int get_client(int fd);
int create_client(char * serv_ip,int port,int timeout);

#define VIDEO_PACKET_MAX_LEN(max)          do{max = (packet_lenght)(1024*1024*3);}while(0)
#define VIDEO_PACKET_MIN_LEN(min)          do{\
    min = sizeof(struct video_reply) < sizeof(struct video_ctl) ? sizeof(struct video_reply) : sizeof(struct video_ctl);\
    min = min < sizeof(struct video_data) ? min : sizeof(struct video_data);\
    min = min < sizeof(struct video_identity) ? min : sizeof(struct video_identity);\
    min = min + sizeof(struct video_head);\
    }while(0)

int video_send_data(int fd,uint8_t const *data,packet_lenght len);
int video_send_identity(int fd,uint16_t id,uint8_t wya,uint8_t const *key,uint8_t key_len);
int video_send_relpy(int fd,uint8_t status,uint16_t reply2seq);
int video_send_ctl(int fd,uint32_t cmd,uint16_t seq,uint8_t const *param,packet_lenght param_len);
int video_send_packet(int fd,uint8_t const *data);

#define VIDEO_RECEIVE_PACKET_MALLOC_MEM_MARK 0x1234567899876543

#define is_identity_packet(pkt) (((struct video_head*)pkt)->type == VIDEO_HEAD_TYPE_IDENTITY?1:0)
#define is_reply_packet(pkt) (((struct video_head*)pkt)->type == VIDEO_HEAD_TYPE_REPLY ?1:0)
#define is_ctl_packet(pkt) (((struct video_head*)pkt)->type == VIDEO_HEAD_TYPE_CTL?1:0)

#define is_identity_collect(pkt) (((struct video_identity *) &((struct video_head *)pkt)->data[0])->wya \
    == VIDEO_IDENTITY_WYA_COLLECT ? 1 : 0)

#define is_identity_view(pkt) (((struct video_identity *) &((struct video_head *)pkt)->data[0])->wya \
    == VIDEO_IDENTITY_WYA_VIEW ? 1 : 0)

#define is_reply_identity_ok(pkt) (((struct video_reply *) &((struct video_head *)pkt)->data[0])->status \
    == VIDEO_REPLY_STATUS_IDENTITY_OK ? 1 : 0)

#define is_reply_data_ok(pkt) (((struct video_reply *) &((struct video_head *)pkt)->data[0])->status \
    == VIDEO_REPLY_STATUS_DATA_OK ? 1 : 0)

#define get_pkt_len(pkt) (((struct video_head *)pkt)->len)
#define get_ctl_cmd(pkt) (((struct video_ctl *) &((struct video_head *)pkt)->data[0])->cmd)
#define get_ctl_seq(pkt) (((struct video_ctl *) &((struct video_head *)pkt)->data[0])->seq)
#define get_data_seq(pkt) (((struct video_data *) &((struct video_head *)pkt)->data[0])->seq)
#define get_data_point(pkt) &(((struct video_data *) &((struct video_head *)pkt)->data[0])->vdata[0])

int rev_pkt_with_mem(int fd,uint8_t *buf,uint32_t buf_size);
int rev_pkt_with_malloc(int fd,uint8_t ** buf,uint32_t *buf_size);

#endif // PACKET_H
