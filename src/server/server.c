#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/epoll.h>

#include "packet.h"
#include "pthreadpool.h"

struct client_capturer{
    uint16_t id;
    struct client_viewer *list;
    uint32_t video_type;
    uint16_t buffer_size;

    uint8_t *buffer;
    uint8_t *buffer_backup;

};

struct client_viewer{
    uint16_t id;
    struct client_capturer *current_capture;
    struct client_capturer *list;
};

struct client{
    int fd;
    int listen;
    int epoll_fd;
    uint8_t client_type;
    union {
        struct client_viewer *viewer;
        struct client_capturer *capturer;
    } client;
    struct client *next;
};

void app(void * ptr)
{
    struct epoll_event event;
    int listen_fd = ((struct client *)ptr)->listen;
    int epoll_fd =  ((struct client *)ptr)->epoll_fd;

    int conn = accept(listen_fd,NULL,NULL);
    event.events = EPOLLIN|EPOLLERR|EPOLLET;
    event.data.fd = conn;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,conn,&event);
}

int main(int argc,char *argv[])
{
	int result;
	int listen_fd;
    unsigned char data[1459200+512];

    listen_fd = create_server(6666);
	if(listen_fd < 0){
		perror(strerror(listen_fd));
		return listen_fd;
	}

    pthreadpool_manager *pth_m;

    pth_m = create_threadpool(10,11);

    struct epoll_event event[10];
    memset(event,0,sizeof(event));
    event[0].events = EPOLLIN|EPOLLERR|EPOLLET;
    event[0].data.fd = listen_fd;

    int fds = 0;
    int conn;
    int epoll_fd= epoll_create(100);

    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_fd,event);

    do{
        fds = epoll_wait(epoll_fd,&event[0],9,-1);

        for(int i=0;i<fds;i++){

            if(event[i].data.fd == listen_fd ){
                printf("listen\n");
                struct client *clt = (struct client *)malloc(sizeof(struct client ));
                clt->listen = listen_fd;
                clt->epoll_fd = epoll_fd;
                add_task_to_threadpool(pth_m,app,clt,0,NULL);
            }else{
                printf("read:%d\n",read(event[i].data.fd,data,1024*3));
            }


        }
    }while(1);

    /*****************************************************************/
    int conn0;
	int v_fd = -1;
	int c_fd = -1;
    while((v_fd == -1)||(c_fd == -1)){
        conn = get_client(listen_fd);
        result = rev_pkt_with_mem(conn,(void*)data,480*640*2+512);
        if(result < 0){
            close(conn);
            conn = -1;
            continue;
        }

        if(is_identity_packet(data) == 0){
            close(conn);
            conn = -1;
            continue;
        }
        printf("socket in\n");
        if((is_identity_collect(data) == 0)&&(is_identity_view(data) == 0)){
            close(conn);
            conn = -1;
            continue;
        }

        if(video_send_relpy(conn,VIDEO_REPLY_STATUS_IDENTITY_OK,0) < 0){
            close(conn);
            conn = -1;
            continue;
        }

        if(is_identity_collect(data) == 1){
            if(c_fd >0) close(c_fd);
            c_fd = conn;
            conn = -1;
            continue;
        }

        if(v_fd >0) close(v_fd);
        v_fd = conn;
        conn = -1;
    }

	while(1){
        video_send_ctl(c_fd,VIDEO_CTL_CMD_REQ_ONE_PACKETDAT,10,NULL,0);
        result = rev_pkt_with_mem(c_fd,data,1459200+512);
        printf("result:%d\n",result);
        printf("%ld\n",(int64_t)get_pkt_len(data));

        video_send_relpy(c_fd,VIDEO_REPLY_STATUS_DATA_OK,get_data_seq(data));
        video_send_packet(v_fd,data);

	}

	return 0;
}
