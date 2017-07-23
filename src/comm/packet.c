#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "packet.h"


int create_server(int port)
{
    int result;
    int listen_fd;
    struct sockaddr_in serv_addr;

    listen_fd = socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd < 0){
        perror(strerror(listen_fd));
        return listen_fd;
    }

    result = 1;
    result = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &result, sizeof(result));
    if(result < 0){
        perror(strerror(result));
        return result;
    }

    result = 1;
    result = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &result, sizeof(result));
    if(result < 0){
        perror(strerror(result));
        return result;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    result = bind(listen_fd,(struct sockaddr*)&serv_addr,sizeof(serv_addr));
    if(result < 0){
        close(listen_fd);
        perror(strerror(result));
        return result;
    }

    result = listen(listen_fd,10);
    if(result < 0){
        close(listen_fd);
        perror(strerror(result));
        return result;
    }

    return listen_fd;
}

int get_client(int fd)
{
    int conn;

    conn = accept(fd,NULL,NULL);
    if(conn < 0){
        perror(strerror(conn));
    }

    return conn;
}

int create_client(char * serv_ip,int port,int timeout)
{
    int result;
    int serv_fd;
    struct sockaddr_in serv_addr;

    serv_fd = socket(AF_INET,SOCK_STREAM,0);
    if(serv_fd < 0){
        perror(strerror(serv_fd));
        return serv_fd;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    result = inet_aton(serv_ip,&serv_addr.sin_addr);
    if(result < 0){
        perror(strerror(result));
        close(serv_fd);
        return result;
    }

    do{
        result = connect(serv_fd,(struct sockaddr*)&serv_addr,sizeof(serv_addr));
        timeout --;
        sleep(1);
    }while((timeout > 0)&&(result < 0));

    if(result < 0){
        perror(strerror(result));
        close(serv_fd);
        return result;
    }

    return serv_fd;
}

static inline int video_write(int fd,char const * data,int size)
{
    int c_write = 0;
    int result = 0;

    if((fd < 0)||(data==NULL)||(size<=0)){
        return -1;
    }

    do{
        result = write(fd,&data[c_write],size -c_write);
        c_write += result;
    }while((result>0) && (c_write<size));
    if(result <= 0){
        return -1;
    }

    return 0;
}
int video_send_packet(int fd,uint8_t const *data)
{
    if(data == NULL){
        return -1;
    }

    return video_write(fd,(void *)data,((struct video_head*)data)->len);
}

int video_send_data(int fd,uint8_t const *data,packet_lenght len)
{
    int result;
    int segment_len;
    uint8_t vadata[sizeof(struct video_identity)+sizeof(struct video_head)];
    struct video_head *vhead = (struct video_head *)vadata;
    struct video_data *vdata = (struct video_data *) &(vhead->data[0]);

    if((data == NULL)||(len == 0)||(fd < 0)){
        return -1;
    }

    vhead->len = sizeof(struct video_head) + sizeof(struct video_data) + len;
    vhead->type = VIDEO_HEAD_TYPE_DATA;
    vdata->seq = 0x0010;
    vdata->vdata_len = len;

    //reduce kernel copy,
    result = video_write(fd,(void*)vadata,sizeof(struct video_head) + sizeof(struct video_data));
    if(result < 0){
        return -1;
    }

    segment_len = 0;
    result = 0;
    while(((len-segment_len) > VIDEO_DATA_SEGMENT_LEN)&&(result >=0)){
        result = video_write(fd,(void *)(&data[segment_len]),VIDEO_DATA_SEGMENT_LEN );
        segment_len += VIDEO_DATA_SEGMENT_LEN;
    }
    if(result < 0){
        return -1;
    }

    return video_write(fd,(void*)(&data[segment_len]),(len-segment_len));

}

int video_send_identity(int fd,uint16_t id,uint8_t wya,uint8_t const *key,uint8_t key_len)
{
    uint8_t data[sizeof(struct video_identity)+sizeof(struct video_head)];
    struct video_head *head;
    struct video_identity *identity;

    if((fd < 0)||(key == NULL)){
        return -1;
    }

    head = (struct video_head *)data;
    head->len = sizeof(data);
    head->type = VIDEO_HEAD_TYPE_IDENTITY;
    identity = (struct video_identity *) &(head->data[0]);
    identity->id = id;
    identity->wya = wya;
    memcpy(identity->key,key,VIDEO_IDENTITY_KEY_SIZE<key_len?VIDEO_IDENTITY_KEY_SIZE:key_len);

    return video_write(fd,(void *)data,head->len);
}

int video_send_ctl(int fd,uint32_t cmd,uint16_t seq,uint8_t const *param,uint32_t param_len)
{

    if((fd < 0)||((param_len > 0)&&(param == NULL))||(param_len > VIDEO_CTL_CMD_PARAM_MAX)){
        return -1;
    }

    struct video_head * head = (struct video_head*)malloc(sizeof(struct video_head)+sizeof(struct video_ctl)+param_len);
    if(head == NULL){
        return -1;
    }

    head->len = sizeof(struct video_head)+sizeof(struct video_ctl)+param_len;
    head->type = VIDEO_HEAD_TYPE_CTL;
    struct video_ctl *ctl = (struct video_ctl *) &(head->data[0]);
    ctl->cmd = cmd;
    ctl->seq = seq;
    ctl->param_len = param_len;
    memcpy((void *) &ctl->param[0],param,param_len);
    int result = video_write(fd,(void *)head,head->len);
    free(head);
    return result;
}

int video_send_relpy(int fd,uint8_t status,uint16_t reply2seq)
{
    uint8_t data[sizeof(struct video_reply)+sizeof(struct video_head)];
    struct video_head *head;
    struct video_reply *reply;

    if(fd < 0){
        return -1;
    }

    head = (struct video_head *)data;
    head->len = sizeof(data);
    head->type = VIDEO_HEAD_TYPE_REPLY;
    reply = (struct video_reply *) &(head->data[0]);
    reply->reply2seq = reply2seq;
    reply->status = status;

    return video_write(fd,(void *)data,head->len);
}

static inline int video_read(int fd, char * data,int size)
{
    int c_read = 0;
    int result = 0;

    if((fd<0)||(data==NULL)||(size<=0)){
        return -1;
    }

    do{
        result = read(fd,(void *)&data[c_read],size-c_read);
        c_read += result;
    }while((c_read < size)&&(result > 0));
    if(result <= 0){
        return -1;
    }

    return 0;

}

static inline int video_read_aligned(int fd,int size)
{
    int result = 0;
    uint8_t data[1024];

    if(size <= 0){
        return -1;
    }

    while((size>1024)&&(result >=0)){
        result =video_read(fd, (void*)data,1024);
        size -=1024;
    }

    if(result >= 0){
        result = video_read(fd, (void*)data,size);
    }

    return result;

}

int rev_pkt_with_mem(int fd,uint8_t *buf,uint32_t buf_size)
{
    packet_lenght pk_len;
    int result = 0;

    if((fd < 0)||(buf == NULL)||(buf_size ==0)){
        return -1;
    }

    result = video_read(fd,(void *)buf,sizeof(packet_lenght));
    if(result < 0){
        return -1;
    }
    pk_len = *((packet_lenght *) buf);

    packet_lenght pk_lt;
    VIDEO_PACKET_MAX_LEN(pk_lt);
    if(pk_len > pk_lt){
        return -1;
    }

    VIDEO_PACKET_MIN_LEN(pk_lt);
    if(pk_len < pk_lt){
        return -1;
    }

    if(pk_len > buf_size){
        video_read_aligned(fd,pk_len-sizeof(packet_lenght));
        return -1;
    }

    ((struct video_head *)buf)->len = pk_len;
    result = video_read(fd, (char *) &(((struct video_head *)buf)->type),((struct video_head *)buf)->len - sizeof(packet_lenght));
    if(result < 0){
        return -1;
    }

    return 0;
}

int rev_pkt_with_malloc(int fd,uint8_t ** buf,uint32_t *buf_size)
{
    packet_lenght pk_len;
    int result = 0;

    if((fd < 0)||(buf == NULL)||(buf_size == NULL)){
        return -1;
    }

    result = video_read(fd,(void *)&pk_len,sizeof(packet_lenght));
    if(result < 0){
        return -1;
    }


    packet_lenght pk_lt;
    VIDEO_PACKET_MAX_LEN(pk_lt);
    if(pk_len > pk_lt){
        return -1;
    }

    VIDEO_PACKET_MIN_LEN(pk_lt);
    if(pk_len < pk_lt){
        return -1;
    }

    uint64_t *mark = (uint64_t *)malloc(sizeof(uint64_t) + pk_len + 4);
    if(mark == NULL) {
        video_read_aligned(fd,pk_len-sizeof(packet_lenght));
        return -1;
    }

    *mark = VIDEO_RECEIVE_PACKET_MALLOC_MEM_MARK;
    *buf = (uint8_t *) &mark[1];
    *buf_size = pk_len;

    ((struct video_head *)buf)->len = pk_len;
    result = video_read(fd, (void *) &(((struct video_head *)buf)->type),pk_len - sizeof(packet_lenght));
    if(result < 0){
        return -1;
    }

    return 0;
}

void rev_pkt_malloc_destory(void * p)
{
    if(*((uint64_t *)p -1) == VIDEO_RECEIVE_PACKET_MALLOC_MEM_MARK){
        free((void *)((uint64_t *)p -1));
    }
}

