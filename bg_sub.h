 #ifdef __cplusplus
 #define EXTERNC extern "C"
 #else
 #define EXTERNC
 #endif

#define BUFFER_W 1920
#define BUFFER_H 1920

#define CNT_RAW8_BUF 3686400  //1920 X 1920
#define SUBTRACTED_MARK   0x00

EXTERNC void subtract_background_w_adjust_8(unsigned char *input, unsigned char *bg, int threshold);
EXTERNC unsigned char *create_avg_img_8(char *img_list[], int cnt);
