
#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1920
#define RCV_BUFFER_SIZE  5*1024*1024*100 //1024 frames
#define RAW_FRAME_SIZE 4608000
#define RGB24_BUFFER_SIZE FRAME_WIDTH*FRAME_HEIGHT*3  //3 bytes for each pixel


typedef struct _FrameBuf{
    int sd;
    int head;
    int tail;
    int rcv_running;
    char *rgb24buf;  // Buffer for saving RGB24 format from RAW(raw_buf)
    char *raw_buf;  // Buffer to get one from from rcv_buf
    char *rcv_buf;  // Buffer for saving frames from client(Pi)
    void *puser;  //GTK Display user specific data
}FrameBuf;

FrameBuf *init_buffer(void *p);
int save_data(FrameBuf *pbuf, char *data, int len);
int get_data(FrameBuf *pbuf, char *p, int len);
int get_buf_size(FrameBuf *pbuf);
int get_data_cnt(FrameBuf *pbuf);
char *get_a_frame(FrameBuf *pbuf, char *frame, int len);
void save_buf_to_file(char *buf, int len, char *fname);
void save_buf_as_ppm(char *buf, int len, char *fname);
void release_buffer(FrameBuf *pbuf);
