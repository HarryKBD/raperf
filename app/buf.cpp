#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>


#include "buf.h"


static FrameBuf *fbuf;

FrameBuf *init_buffer(){
    fbuf = (FrameBuf *)malloc(sizeof (FrameBuf));
    fbuf->head = 0;
    fbuf->tail = 0;
    fbuf->buf = (char *)malloc(BUFFER_SIZE);
    return fbuf;
}


void save_buf_to_file(char *buf, int len, char *fname){

    FILE *fp = fopen(fname, "wb");
    int wlen;
    if(!fp){
        printf("save buf file open error\n");
        return;
    }

    wlen = fwrite(buf, len, 1, fp);

    if(wlen != len){
        printf("save file len error written: %d expected: %d\n", wlen, len);
    }

    printf("saveing buf to file %s done\n", fname);
    fclose(fp);
}

void save_buf_as_ppm(char *buf, int len, char *fname){

    FILE *fp = fopen(fname, "wb");
    int wlen;
    if(!fp){
        printf("save buf file open error\n");
        return;
    }

    fprintf(fp, "P6\n%d %d\n255\n", 1920, 1920);

    wlen = fwrite(buf, len, 1, fp);

    if(wlen != len){
        printf("save file len error written: %d expected: %d\n", wlen, len);
    }

    printf("saveing buf to file %s done\n", fname);
    fclose(fp);
}



void dump_packet(FrameBuf *fbuf, unsigned char *data, int len){
    
    int i;
    int head = fbuf->head;
    int tail = fbuf->tail;

    printf("---------------------------- HEAD %d   TAIL %d ---------------------------\n", head, tail);
    for(i=0; i < len; i++){
        printf("%02X ", data[i]);
        if((i+1)%32 == 0) printf("\n");
    }

    printf("\n---------------------------------------------- ---------------------------\n");

}

int save_data(FrameBuf *fbuf, char *data, int len){
    int e;
    char *buf = fbuf->buf;
    //dump_packet((unsigned char *)data, len);


    if(fbuf->head < fbuf->tail){
        e = fbuf->tail - fbuf->head;
        if(e < len){
            printf("avail space %d. less than input len %d\n", e, len);
            return -1;
        }
        memcpy(buf+fbuf->head, data, len);
        fbuf->head += len;
    }
    else{
        int rleft = BUFFER_SIZE - fbuf->head;
        if(rleft >= len){
            memcpy(buf+fbuf->head, data, len);
            fbuf->head += len;
        }
        else{
            e = rleft + fbuf->tail;
            if(e < len){
                printf("avail space %d. less than input len %d\n", e, len);
                return -1;
            }
            //printf("copy two times tail %d, head %d, rleft %d\n", fbuf->tail, fbuf->head, rleft);
            memcpy(buf+fbuf->head, data, rleft);
            memcpy(buf, data+rleft, len-rleft);
            fbuf->head = len - rleft;
            //printf("New Head %d\n", fbuf->head);
        }
    }
    return len;
}

int get_data(FrameBuf *fbuf, char *p, int len){
    int c;
    char *buf = fbuf->buf;

    if(fbuf->head == fbuf->tail) return 0;

    if(fbuf->head > fbuf->tail){
        c = fbuf->head - fbuf->tail;
        if (c > len){
            memcpy(p, buf+fbuf->tail, len);
            fbuf->tail += len;
            return len;
        }else{
            memcpy(p, buf+fbuf->tail, c);
            fbuf->tail += c;
            return c;
        }
    }
    else{
        int rcnt = BUFFER_SIZE - fbuf->tail;
        if(rcnt > len){
            memcpy(p, buf+fbuf->tail, len);
            fbuf->tail += len;
            return len;
        }else{
            int left;
            memcpy(p, buf+fbuf->tail, rcnt);
            left = len - rcnt;
            if(fbuf->head >= left){
                memcpy(p+rcnt, buf, left);
                fbuf->tail = left;
                return len;
            }else{
                printf("something\n");
                memcpy(p+rcnt, buf, fbuf->head);
                fbuf->tail = fbuf->head;
                return rcnt+fbuf->head;
            }
        }
    }
}

char FRAME_MAGIC[] = {0x33, 0x44, 0x55, 0x44, 0x44, 0x77, 0x77, 0x27};
int MAGIC_LEN  = sizeof(FRAME_MAGIC);
char temp_frame[8];


char *get_a_frame(FrameBuf *fbuf, char *frame, int len){
    int read_len = get_data(fbuf, temp_frame, 8);

    if(read_len != 8){
        printf("Critical Error.... \n");
        return NULL;
    }

    //dump_packet(fbuf, (unsigned char *)temp_frame, 8);

    if(memcmp((void *)FRAME_MAGIC, temp_frame, MAGIC_LEN)){
        printf("Critical Error2.... \n");
    dump_packet(fbuf, (unsigned char *)temp_frame, 8);
        getchar();
        return NULL;
    }

    read_len = get_data(fbuf, frame, len);

    if(read_len != len){
        printf("Critical Error3\n");
        return NULL;
    }

    //printf("get_a_frame frame len %d\n", read_len);

    return frame;
}

int get_data_cnt(FrameBuf *fbuf){
    int c;

    if(fbuf->head == fbuf->tail) return 0;

    if(fbuf->head > fbuf->tail){
        c = fbuf->head - fbuf->tail;
        return c;
    }
    else{
        int rcnt = BUFFER_SIZE - fbuf->tail;
        return rcnt+fbuf->head;
    }
}

int cmp_array(char *s1, char *s2, int len){
    int i;
    for(i=0; i < len; i++){
        if(*s1++ != *s2++) return i;
    }

    return 0;
}
