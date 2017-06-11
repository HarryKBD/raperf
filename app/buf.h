
#define BUFFER_SIZE  5*1024*1024*100 //1024 frames
//#define BUFFER_SIZE  5*1024*1024*10 //1024 frames


typedef struct _FrameBuf{
    int sd;
    int head;
    int tail;
    char *buf;
}FrameBuf;

FrameBuf *init_buffer();
int save_data(FrameBuf *pbuf, char *data, int len);
int get_data(FrameBuf *pbuf, char *p, int len);
int get_buf_size(FrameBuf *pbuf);
int get_data_cnt(FrameBuf *pbuf);
char *get_a_frame(FrameBuf *pbuf, char *frame, int len);
void save_buf_to_file(char *buf, int len, char *fname);
void save_buf_as_ppm(char *buf, int len, char *fname);
