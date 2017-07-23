#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include "packet.h"

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
	
    int conn;
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
