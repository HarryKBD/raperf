#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>

#define SCREEN_W 1920
#define SCREEN_H 1920

#define BUFFER_W 2400   //SCREEN_W * 5 / 4 for 10 bit RAW images
#define BUFFER_H 1920

#define CNT_RAW8_BUF 3686400  //1920 X 1920
#define CNT_RAW10_BUF 4608000 //2400 X 1920

char *filename1[] = {"obj1_bg1.raw", "obj1_bg2.raw", "obj1_bg3.raw", "obj1_bg4.raw", "obj1_bg5.raw" };
char *filename2[] = {"obj2_bg1.raw", "obj2_bg2.raw", "obj2_bg3.raw", "obj2_bg4.raw", "obj2_bg5.raw" };

struct timeval tvstart, tvend;

unsigned int diff[20];
unsigned int diff_total = 0;

//to generate background image
//1. accumulate 5 images to the accumuate(int) and square(int) array. 
//the final result would be avg(char) + devation(char: round down)
//

unsigned char temp_read[BUFFER_W * BUFFER_H];

void print_time_diff(struct timeval *start, struct timeval *end){
    long lstart = start->tv_sec * 1000000 + start->tv_usec;
    long lend = end->tv_sec * 1000000 + end->tv_usec;

    printf("Time diff(start %d.%d, end %d.%d :  %2.3f\n", 
            start->tv_sec, start->tv_usec, end->tv_sec, end->tv_usec, (lend-lstart)/1000000.0);
    return;
}

void start(){
    gettimeofday(&tvstart, NULL);
}

void stop(int print_result){
    gettimeofday(&tvend, NULL);
    if(print_result){
        print_time_diff(&tvstart, &tvend);
    }
}

void init() {
    return;
}

void dump_buf(char *msg, char *data, int size){
    if(msg != NULL){
        printf("-------------- DUMP (%s) -----------------------\n");
    }
    int i;
    for(i=0; i < size; i++){
        if(i%15 == 0) printf("\n");
        printf("%02X ", data[i]);
    }
    printf("------------------------------------------------\n");
    return;
}

void save_buf(char *filename, char *buf, int size){
    FILE *fp;
    fp = fopen(filename, "wb");
    fwrite(buf, 1, size, fp);
    fclose(fp);
    return;
}

int extract_name(char *fname, char *name){
    int len = strlen(fname);
    int i;
    int dot_idx = -1;

    for(i=len-1; i > 0; i--){
        if(fname[i] == '.'){
            dot_idx = i;
            break;
        }
    }

    if(dot_idx > 0){
        memcpy(&name[0], &fname[0], dot_idx);
        name[dot_idx] = 0; 
        printf("found idx%d => %s \n", dot_idx, name);
        return 1;
    }
    else{
        name[0] = 0;
        return 0;
    }
} 
unsigned char *create_fg_image( unsigned char *input, unsigned char *bg, int threshold){
    int subtracted_cnt = 0;
    int i;
    unsigned char *out = (unsigned char *)malloc(CNT_RAW10_BUF * sizeof(unsigned char));

    memset(out, 0, CNT_RAW10_BUF * sizeof(unsigned char));
    
	for(i=0; i < CNT_RAW10_BUF; i+=5){
		(abs(input[i] - bg[i]) > threshold)? out[i] = input[i]: subtracted_cnt++;
		(abs(input[i+1] - bg[i+1]) > threshold)? out[i+1] = input[i+1]:subtracted_cnt++;
		(abs(input[i+2] - bg[i+2]) > threshold)? out[i+2] = input[i+2]:subtracted_cnt++;
		(abs(input[i+3] - bg[i+3]) > threshold)? out[i+3] = input[i+3]:subtracted_cnt++;
	}

    printf("%d (%2.1f %) of Total %d is subtracted.\n", 
            subtracted_cnt, (subtracted_cnt/(double)CNT_RAW8_BUF)*100.0, CNT_RAW8_BUF);
    //post processing is required
    return out;
    
}

void subtract_background(unsigned char *input, unsigned char *bg, int threshold){
    int fg_cnt = 0;
    int i;
    
	for(i=0; i < CNT_RAW10_BUF; i+=5){
		(abs(input[i] - bg[i]) < threshold)? input[i] = 0x00:fg_cnt++;
		(abs(input[i+1] - bg[i+1]) < threshold)? input[i+1] = 0x00:fg_cnt++;
		(abs(input[i+2] - bg[i+2]) < threshold)? input[i+2] = 0x00:fg_cnt++;
		(abs(input[i+3] - bg[i+3]) < threshold)? input[i+3] = 0x00:fg_cnt++;
        input[i+4] = 0x00; //just clear the 5th byte.
	}
 
    //post processing is required
    printf("%d (%2.1f %) of Total %d is subtracted.\n", 
            (CNT_RAW8_BUF-fg_cnt), ((CNT_RAW8_BUF-fg_cnt)/(double)CNT_RAW8_BUF)*100.0, CNT_RAW8_BUF);
    return;
}

void adjust_edge_pixel(unsigned char *input, unsigned char *org_image, int wide){
    int i, j;


    unsigned char *tmp = (unsigned char *)malloc(CNT_RAW10_BUF * sizeof(unsigned char));
    memcpy(tmp, input, CNT_RAW10_BUF * sizeof(unsigned char));
    
	for(i=2; i < BUFFER_H - 2; i++){
		for(j=2; j < BUFFER_W - 2; j++){
			if((((i*BUFFER_W) + j)+1)%5 == 0){
				continue;
			}

            if(input[i*BUFFER_W + j] != 0x00){
				//copy
				tmp[(i-1)*BUFFER_W + (j-1)] = org_image[(i-1)*BUFFER_W + (j-1)];
				tmp[(i-1)*BUFFER_W + (j  )] = org_image[(i-1)*BUFFER_W + (j  )];
				tmp[(i-1)*BUFFER_W + (j+1)] = org_image[(i-1)*BUFFER_W + (j+1)];

				tmp[(i)*BUFFER_W + (j-1)] = org_image[(i)*BUFFER_W + (j-1)];
				tmp[(i)*BUFFER_W + (j+1)] = org_image[(i)*BUFFER_W + (j+1)];

				tmp[(i+1)*BUFFER_W + (j-1)] = org_image[(i+1)*BUFFER_W + (j-1)];
				tmp[(i+1)*BUFFER_W + (j  )] = org_image[(i+1)*BUFFER_W + (j  )];
				tmp[(i+1)*BUFFER_W + (j+1)] = org_image[(i+1)*BUFFER_W + (j+1)];


                if(wide == 1){
                    tmp[(i-1)*BUFFER_W + (j-2)] = org_image[(i-1)*BUFFER_W + (j-2)];
                    tmp[(i-1)*BUFFER_W + (j+2)] = org_image[(i-1)*BUFFER_W + (j+2)];

                    tmp[(i)*BUFFER_W + (j-2)] = org_image[(i)*BUFFER_W + (j-2)];
                    tmp[(i)*BUFFER_W + (j+2)] = org_image[(i)*BUFFER_W + (j+2)];

                    tmp[(i+1)*BUFFER_W + (j-2)] = org_image[(i+1)*BUFFER_W + (j-2)];
                    tmp[(i+1)*BUFFER_W + (j+2)] = org_image[(i+1)*BUFFER_W + (j+2)];

                    tmp[(i-2)*BUFFER_W + (j-2)] = org_image[(i-2)*BUFFER_W + (j-2)];
                    tmp[(i-2)*BUFFER_W + (j-1)] = org_image[(i-2)*BUFFER_W + (j-1)];
                    tmp[(i-2)*BUFFER_W + (j  )] = org_image[(i-2)*BUFFER_W + (j  )];
                    tmp[(i-2)*BUFFER_W + (j+1)] = org_image[(i-2)*BUFFER_W + (j+1)];
                    tmp[(i-2)*BUFFER_W + (j+2)] = org_image[(i-2)*BUFFER_W + (j+2)];

                    tmp[(i+2)*BUFFER_W + (j-2)] = org_image[(i+2)*BUFFER_W + (j-2)];
                    tmp[(i+2)*BUFFER_W + (j-1)] = org_image[(i+2)*BUFFER_W + (j-1)];
                    tmp[(i+2)*BUFFER_W + (j  )] = org_image[(i+2)*BUFFER_W + (j  )];
                    tmp[(i+2)*BUFFER_W + (j+1)] = org_image[(i+2)*BUFFER_W + (j+1)];
                    tmp[(i+2)*BUFFER_W + (j+2)] = org_image[(i+2)*BUFFER_W + (j+2)];
                }
			}
		}
	}

    memcpy(input, tmp, CNT_RAW10_BUF * sizeof(unsigned char));
    free(tmp);
}


void convert_to_ppm(char *fname){
    char test_cmd[500];
    sprintf(test_cmd, "debayer -w 1920 -h 1920 -cw 1920 -ch 1920 -aw 1920 -ah 1920 -i %s -o %s.ppm &> /dev/null",
            fname, fname);
    printf("Executing: %s\n", test_cmd);
    system(test_cmd);
    return;
}

unsigned char *get_foreground_image(char *fname, unsigned char *bg, int threshold){
	//todo: devation, currently not used.
	
	int len;
	int i, j;
    char *output;
    char save_fname[100];
    char name_only[50];
    
    if(!extract_name(fname, name_only)){
        printf("Cannot extract filename\n");
        return NULL;
    } 

	FILE *fp;
	fp = fopen(fname, "rb");
	if(!fp){
		printf("file open error\n");
		return NULL;
	}

	len = fread(temp_read, 1, CNT_RAW10_BUF, fp);
    unsigned char *backup = (unsigned char *)malloc(CNT_RAW10_BUF * sizeof(unsigned char));
    memcpy(backup, temp_read, CNT_RAW10_BUF * sizeof(unsigned char));
	fclose(fp);

    start();
    output = create_fg_image(temp_read, bg, threshold);
    stop(1);
    sprintf(save_fname, "result1_%s_th%d.raw", name_only, threshold);
    printf("saving result %s\n", save_fname);
    save_buf(save_fname, output, CNT_RAW10_BUF);
    convert_to_ppm(save_fname);

    start();
    adjust_edge_pixel(output, backup, 0);
    stop(1);
    sprintf(save_fname, "result1_%s_th%d_adjusted.raw", name_only, threshold);
    save_buf(save_fname, output, CNT_RAW10_BUF);
    convert_to_ppm(save_fname);
 

    start();
    subtract_background(temp_read, bg, threshold);
    stop(1);
    sprintf(save_fname, "result2_%s_th%d.raw", name_only, threshold);
    save_buf(save_fname, temp_read, CNT_RAW10_BUF);
    convert_to_ppm(save_fname);

    start();
    adjust_edge_pixel(temp_read, backup, 0);
    stop(1);
    sprintf(save_fname, "result2_%s_th%d_adjusted.raw", name_only, threshold);
    save_buf(save_fname, temp_read, CNT_RAW10_BUF);
    convert_to_ppm(save_fname);
    
    printf("background subtraction done\n");
    free(backup);
	return;
}


/*
 * input: img_list: list of images (RAW10 format: 2400 X 1920)
 *        cnt: number of images
 * return: RAW10 format raw buffer size. (2400 X 1920)
 */

unsigned char *create_avg_img(char *img_list[], int cnt){
	FILE *fp;
    int i, j;
    int len, file_cnt = 0;
    unsigned char *out = malloc(CNT_RAW10_BUF * sizeof(unsigned char));;
    unsigned short *temp_acc = malloc(CNT_RAW10_BUF * sizeof(unsigned short));

    memset(temp_acc, 0, CNT_RAW10_BUF * sizeof(unsigned short));

    for (i = 0; i < cnt; i++) {
        fp = fopen(img_list[i], "rb");
		if(!fp){	
			printf("file open error\n");
			continue;
		}

		len = fread(temp_read, 1, sizeof(temp_read), fp);
		if(len != sizeof(temp_read)){
			printf("read error. ret size %d\n", len);
			continue;
		}
    
    	for(j=0; j < CNT_RAW10_BUF; j++){
            temp_acc[j] += temp_read[j];
    	}
		file_cnt++;
		fclose(fp);
    }

	for(i=0; i < CNT_RAW10_BUF; i+=5){
        out[ i ] = (unsigned char)(temp_acc[ i ]/(float)cnt);
        out[i+1] = (unsigned char)(temp_acc[i+1]/(float)cnt);
        out[i+2] = (unsigned char)(temp_acc[i+2]/(float)cnt);
        out[i+3] = (unsigned char)(temp_acc[i+3]/(float)cnt);
        out[i+4] = 0x00;
    }

    free(temp_acc);
    return out;
}

int main(int argc, char **argv){

    int i, w, h;
	int len;
	int file_cnt = 0;
    init();
    char *avg_bg;

    int threshold = 20;

    start();
    avg_bg = create_avg_img(filename1, 5);
    stop(1);
    printf("creating avg image done\n");

    get_foreground_image("obj1.raw", avg_bg, 5);
    get_foreground_image("obj1.raw", avg_bg, 10);
    get_foreground_image("obj1.raw", avg_bg, 15);
    get_foreground_image("obj1.raw", avg_bg, 20);
    return 1;
}

