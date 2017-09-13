#include "packets.h"

static inline int v_write(int fd,char const * data,int size)
{
    if((fd < 0)||(data == NULL)||(size < 0)) return -1;

    int i=0;
    int result;
    do{
        result = write(fd,(const void *)(data+i),size-i);
        i += result;
        if((result > 0)&&(i < size)){
            usleep(1000);
        }else{
            break;
        }
    }while(1);

    if(result > 0){
        return size;
    }

    return result;
}

int send_
