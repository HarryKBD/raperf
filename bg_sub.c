#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <math.h>
#include <sys/time.h>

#include "bg_sub.h"

void update_neighbor_pixels(int i, unsigned char *input, unsigned char *org_image){
    if((i % BUFFER_W) && (i >= BUFFER_W)){
        int pos = i - 1;
        if(input[pos] == SUBTRACTED_MARK) input[pos] = org_image[pos];

        pos = i - BUFFER_W;
        if(input[pos] == SUBTRACTED_MARK) input[pos] = org_image[pos];

        pos = pos - 1;
        if(input[pos] == SUBTRACTED_MARK) input[pos] = org_image[pos];
    }

}

void subtract_background_w_adjust_8(unsigned char *input, unsigned char *bg, int threshold){
    int i;
    unsigned char *backup = (unsigned char *)malloc(CNT_RAW8_BUF);
    memcpy(backup, input, CNT_RAW8_BUF);
    
	for(i=0; i < CNT_RAW8_BUF; i+=4){
		(abs(input[i] - bg[i]) <= threshold)? input[i] = SUBTRACTED_MARK:update_neighbor_pixels(i, input, backup);
		(abs(input[i+1] - bg[i+1]) <= threshold)? input[i+1] = SUBTRACTED_MARK:update_neighbor_pixels(i+1, input, backup);
		(abs(input[i+2] - bg[i+2]) <= threshold)? input[i+2] = SUBTRACTED_MARK:update_neighbor_pixels(i+2, input, backup);
		(abs(input[i+3] - bg[i+3]) <= threshold)? input[i+3] = SUBTRACTED_MARK:update_neighbor_pixels(i+3, input, backup);
	}
 
    free(backup);
    return;
}

/*
 * input: img_list: list of images (RAW8 format: 1920 X 1920)
 *        cnt: number of images
 * return: RAW8 format raw buffer size. (average image) (1920 X 1920)
 */

unsigned char *create_avg_img_8(char *img_list[], int cnt){
	FILE *fp;
    int i, j;
    int len, file_cnt = 0;
    unsigned char *out = malloc(CNT_RAW8_BUF * sizeof(unsigned char));

    unsigned char *buf_read = malloc(CNT_RAW8_BUF * sizeof(unsigned char));
    unsigned short *temp_acc = malloc(CNT_RAW8_BUF * sizeof(unsigned short));

    memset(temp_acc, 0, CNT_RAW8_BUF * sizeof(unsigned short));

    for (i = 0; i < cnt; i++) {
        fp = fopen(img_list[i], "rb");
		if(!fp){	
			printf("file open error\n");
			continue;
		}

		len = fread(buf_read, 1, CNT_RAW8_BUF, fp);
		if(len != CNT_RAW8_BUF){
			printf("read error. ret size %d\n", len);
			continue;
		}
    
    	for(j=0; j < CNT_RAW8_BUF; j++){
            temp_acc[j] += buf_read[j];
    	}
		file_cnt++;
		fclose(fp);
    }

	for(i=0; i < CNT_RAW8_BUF; i+=4){
        out[ i ] = (unsigned char)(temp_acc[ i ]/(float)cnt);
        out[i+1] = (unsigned char)(temp_acc[i+1]/(float)cnt);
        out[i+2] = (unsigned char)(temp_acc[i+2]/(float)cnt);
        out[i+3] = (unsigned char)(temp_acc[i+3]/(float)cnt);
    }

    free(temp_acc);
    free(buf_read);
    return out;
}


