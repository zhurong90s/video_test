#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include "packet.h"

int main(int argc,char *argv[])
{
	int result;
    int serv_fd = -1;
    char rec[1024];

    do{
        serv_fd = create_client("127.0.0.1",6666,10);
        if(serv_fd < 0){
            perror(strerror(serv_fd));
            return serv_fd;
        }

        result = video_send_identity(serv_fd,0x1111,VIDEO_IDENTITY_WYA_COLLECT,"12345",6);
        if(result < 0){
            close(serv_fd);
            serv_fd = -1;
            continue;
        }

        result = rev_pkt_with_mem(serv_fd,(void*)rec,1024);
        if(result < 0){
            close(serv_fd);
            serv_fd = -1;
            continue;
        }

        if(is_reply_packet(rec) == 0){
            close(serv_fd);
            serv_fd = -1;
            continue;
        }

        if(is_reply_identity_ok(rec) == 0){
            close(serv_fd);
            serv_fd = -1;
            continue;
        }

    }while(serv_fd < 0);

	int video_fd;
	video_fd = open("/dev/video0",O_RDWR);
	if(video_fd < 0){
		perror(strerror(video_fd));
		return video_fd;
	}

    struct v4l2_format c_fmt;
    memset(&c_fmt,0,sizeof(struct v4l2_format));
    c_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    c_fmt.fmt.pix.pixelformat = 0x33424752;
    c_fmt.fmt.pix.width = 800;
    c_fmt.fmt.pix.height = 600;
    c_fmt.fmt.pix.colorspace=V4L2_COLORSPACE_SRGB;

    result = ioctl(video_fd,VIDIOC_TRY_FMT,&c_fmt);
    if(result < 0){
        perror(strerror(video_fd));
        return result;
    }

	struct v4l2_requestbuffers c_buf_req;
	struct v4l2_buffer c_buf;
	unsigned char * p_buf = NULL;
	unsigned int buf_len = 0;
	
	//requst buffer
	c_buf_req.count = 1;
	c_buf_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	c_buf_req.memory = V4L2_MEMORY_MMAP;
	result = ioctl(video_fd,VIDIOC_REQBUFS,&c_buf_req);
	if(result < 0){
		perror(strerror(video_fd));
        return result;
	}
	
	//queue buffer
	c_buf.index = 0;
	c_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 
	c_buf.memory = V4L2_MEMORY_MMAP;
	result = ioctl(video_fd,VIDIOC_QUERYBUF,&c_buf);
	if(result < 0){
		perror(strerror(video_fd));
        return result;
	}
	buf_len = c_buf.length;
	p_buf = (unsigned char *)mmap(NULL,c_buf.length,PROT_READ,MAP_SHARED,video_fd,c_buf.m.offset);
	
	enum v4l2_buf_type v4l2type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 
	result = ioctl(video_fd,VIDIOC_QBUF,&c_buf);
	result = ioctl(video_fd,VIDIOC_STREAMON,&v4l2type);
	result = ioctl(video_fd,VIDIOC_DQBUF,&c_buf);

	while(1){
        do{
            result = rev_pkt_with_mem(serv_fd,(void *)rec,1024);
            if(result < 0) continue;

            if(is_ctl_packet(rec) == 0){
                continue;
            }

            if(get_ctl_cmd(rec) != VIDEO_CTL_CMD_REQ_ONE_PACKETDAT){
                continue;
            }
        }while(result < 0);

		result = ioctl(video_fd,VIDIOC_QBUF,&c_buf);
		result = ioctl(video_fd,VIDIOC_DQBUF,&c_buf);
        do{
            video_send_data(serv_fd,(void *)p_buf,buf_len);

            result = rev_pkt_with_mem(serv_fd,(void *)rec,1024);
            if(result < 0) continue;
            if(is_reply_packet(rec) == 0) continue;
            if(is_reply_data_ok(rec) == 0) continue;

        }while(result < 0);
	}

	result = ioctl(video_fd,VIDIOC_STREAMOFF, &v4l2type);
	munmap((void *)p_buf,buf_len);
	close(serv_fd);
	close(video_fd);

	return 0;
}
