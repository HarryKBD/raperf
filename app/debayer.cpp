#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <udt.h>
#include "debayer.h"

static void adjust_raw_format(char *raw);
static void convertRGGB(unsigned short * data, unsigned char * rgb);
static void applyWeight(unsigned char * data);

static unsigned short adjust_buf[1920 * 1920];


long int totalR = 0;
long int totalG = 0;
long int totalB = 0;


int convert_raw_to_rgb24(char *raw, char *rgb24buf){

    totalR = totalG = totalB = 0;

    adjust_raw_format(raw);
    convertRGGB(adjust_buf, (unsigned char *)rgb24buf);
//    applyWeight((unsigned char *)rgb24buf);

    return 1;
}

int clamp(int color){
    if(color < 0) return 0;
    if(color > 255) return 255;
    return color;
}


static void applyWeight(unsigned char * data)
{
    float weightGreen = 0;
    float weightRed = 0;
    float weightBlue = 0;
    int height = 1920;
    int width = 1920;
    long int totalHighest = 0;
    totalHighest = totalG;
    if(totalG > totalHighest)
        totalHighest = totalG;
    if(totalB > totalHighest)
        totalHighest = totalB;
    
    weightGreen = (float)totalHighest/(float)totalG;
    weightRed = (float)totalHighest/(float)totalR;
    weightBlue = (float)totalHighest/(float)totalB;
    
    printf("Weight R %f G %f B %f\n", weightRed, weightGreen, weightBlue);
    int index = 0;
    for(index = 0 ; (index+2) < (height*width*3) ; index +=3)
    {
        data[index] = clamp(data[index]*weightRed);
        data[index+1] = clamp(data[index+1]*weightGreen);
        data[index+2] = clamp(data[index+2]*weightBlue);
    }
}






static void adjust_raw_format(char *raw){
    int row, col;
    int x = 0;
    int y = 0;
    int shapedH = 1920;
    int shapedW = 2400;
    int height = 1920;
    int width = 1920;
    char *data = raw;
    unsigned short *data2 = adjust_buf;

    for(row = 0 ; row < shapedH ; row++)
    {
        x = 0;
        for(col = 0 ; col+4 < shapedW ; col+=5)
        {
            unsigned short byte1 = ((unsigned short)data[row*shapedW + col]<<2) +
            (((unsigned short)data[row*shapedW + col+4] & 0xC0) >> 6);
            
            unsigned short byte2 = ((unsigned short)data[row*shapedW + col+1] << 2) +
            (((unsigned short)data[row*shapedW + col+4] & 0x30) >> 4);
            
            unsigned short byte3 = ((unsigned short)data[row*shapedW + col+2] << 2 )+
            (((unsigned short)data[row*shapedW + col+4] & 0xC) >> 2);
            
            unsigned short byte4 = ((unsigned short)data[row*shapedW + col+3] << 2) +
            (((unsigned short)data[row*shapedW + col+4] & 0x3));
            
            data2[(width*y)+x] = byte1;
            data2[(width*y)+x+1] = byte2;
            data2[(width*y)+x+2] = byte3;
            data2[(width*y)+x+3] = byte4;
            x+=4;
        }
        y++;
    }
}

static void convertRGGB(unsigned short * data, unsigned char * rgb)
{
    int x = 0, y = 0;
    int index = 0;
    int height = 1920;
    int width = 1920;

    for(y = 0 ; y < height ; y++)
    {
        for( x = 0 ; x < width ; x++ )
        {
            static unsigned char color[3];
            //printf("Interpolation %d %d ", y, x);
            if(y%2 == 0 && x%2 == 0)
            {
                
                //R pixel
                
                //printf("R\n");
                //create R
                //unsigned char byte_R = (unsigned char)(data2[y*width + x]/4);
                unsigned char byte_R = (unsigned char)(data[y*width + x]/4);
                
                //Create G
                unsigned char byte_G = 0;
                unsigned short tempG = 0;
                unsigned short verticalR = 0;
                unsigned short horizontalR = 0;
                int includeGN = 1;
                int includeGE = 1;
                int includeGW = 1;
                int includeGS = 1;
                if(y-2 >= 0 && y+2 < height && x-2 >= 0 && x+2 < width) {
                    verticalR = abs(data[(y-2)*width + x] - data[(y+2)*width + x]);
                    horizontalR = abs(data[y*width + x -2] - data[y*width + x + 2]);
                    if(verticalR > horizontalR) {
                        includeGN = 0;
                        includeGS = 0;
                    } else if (verticalR < horizontalR) {
                        includeGE = 0;
                        includeGW = 0;
                    }
                }
                int sampleCount = 0;
                if(y-1 >= 0 && includeGN == 1)
                {
                    tempG += data[(y-1)*width + x];
                    sampleCount++;
                }
                if(x+1 < width && includeGE == 1)
                {
                    tempG += data[y*width + x + 1];
                    sampleCount++;
                }
                if(x-1 >= 0 && includeGW == 1)
                {
                    tempG += data[y*width + x-1];
                    sampleCount++;
                }
                if(y + 1 < height && includeGS == 1)
                {
                    tempG += data[(y+1)*width + x];
                    sampleCount++;
                }
                //printf("%d, %d, R   G %\n", y, x);
                byte_G = (unsigned char)((tempG/sampleCount)/4);
                //unsigned char byte_G = (unsigned char)(tempG/sampleCount);
                
                //Create B
                unsigned short tempB = 0;
                sampleCount = 0;
                if(y-1 >= 0 && x-1 >= 0)
                {
                    tempB += data[(y-1)*width + (x-1)];
                    sampleCount++;
                }
                if(y-1 >= 0 && x+1 < width)
                {
                    tempB += data[(y-1)*width + (x+1)];
                    sampleCount++;
                }
                if(y+1 < height && x-1 >= 0)
                {
                    tempB += data[(y+1)*width + (x-1)];
                    sampleCount++;
                }
                if( y+1 < height && x+1 < width)
                {
                    tempB += data[(y+1)*width + (x+1)];
                    sampleCount++;
                }
                //unsigned char byte_B = (unsigned char)(tempB/sampleCount)/4;
                unsigned char byte_B = (unsigned char)((tempB/sampleCount)/4);
                
                color[0] = byte_R;
                color[1] = byte_G;
                color[2] = byte_B;
                
                //(void)fwrite(color, 1, 3, out);
            }
            else if(y%2 == 0 && x%2 == 1)
            {
                //G1 pixel
                //printf("G1\n");
                //create R
                unsigned short tempR = 0;
                int sampleCount = 0;
                if(x-1 >= 0) {
                    tempR += data[y*width + (x-1)];
                    sampleCount++;
                }
                if(x+1 < width)
                {
                    tempR += data[y*width + (x+1)];
                    sampleCount++;
                }
                //unsigned char byte_R = (unsigned char)(tempR/sampleCount)/4;
                unsigned char byte_R = (unsigned char)((tempR/sampleCount)/4);
                
                //Create G
                //unsigned char byte_G = (unsigned char)data2[y*width + x]/4;
                unsigned char byte_G = (unsigned char)((data[y*width + x])/4);
                
                
                //Create B
                unsigned short tempB = 0;
                sampleCount = 0;
                if(y-1 >= 0)
                {
                    tempB += data[(y-1)*width + x];
                    sampleCount++;
                }
                if(y+1 < height)
                {
                    tempB += data[(y+1)*width + x];
                    sampleCount++;
                }
                //unsigned char byte_B = (unsigned char)tempB/sampleCount/4;
                unsigned char byte_B = (unsigned char)((tempB/sampleCount)/4);
                
                color[0] = byte_R;
                color[1] = byte_G;
                color[2] = byte_B;
                //(void)fwrite(color, 1, 3, out);
                
            }
            else if(y%2 == 1 && x%2 == 0)
            {
                //G2 pixel
                //printf("G2\n");
                //create R
                unsigned short tempR = 0;
                int sampleCount = 0;
                if(y-1 >= 0)
                {
                    tempR += data[(y-1)*width + x];
                    sampleCount++;
                }
                if(y+1 < height)
                {
                    tempR += data[(y+1)*width + x];
                    sampleCount++;
                }
                //unsigned char byte_R = (unsigned char)(tempR/sampleCount/4);
                unsigned char byte_R = (unsigned char)((tempR/sampleCount)/4);
                
                //Create G
                //unsigned char byte_G = (unsigned char)(data2[y*width + x]/4);
                unsigned char byte_G = (unsigned char)(data[y*width + x]/4);
                
                //Create B
                unsigned short tempB = 0;
                sampleCount = 0;
                
                if(x-1 >= 0)
                {
                    tempB += data[y*width + (x-1)];
                    sampleCount++;
                }
                if(x+1 < width)
                {
                    tempB += data[y*width + (x+1)];
                    sampleCount++;
                }
                //unsigned char byte_B = (unsigned char)(tempB/sampleCount/4);
                unsigned char byte_B = (unsigned char)((tempB/sampleCount)/4);
                
                color[0] = byte_R;
                color[1] = byte_G;
                color[2] = byte_B;
                
            }
            else if(y%2 == 1 && x%2 == 1)
            {
                //B pixel
                //printf("B\n");
                //create R
                unsigned short tempR = 0;
                int sampleCount = 0;
                if(y-1 >= 0 && x-1 >= 0)
                {
                    tempR += data[(y-1) * width + (x-1)];
                    sampleCount++;
                }
                if(y-1 >= 0 && x+1 < width)
                {
                    tempR += data[(y-1)*width + (x+1)];
                    sampleCount++;
                }
                if(y+1 < height && x-1 >= 0)
                {
                    tempR += data[(y+1)*width + (x-1)];
                    sampleCount++;
                }
                if(y+1 < height && x+1 < width)
                {
                    tempR += data[(y+1)*width + (x+1)];
                    sampleCount++;
                }
                //unsigned char byte_R = (unsigned char)(tempR/sampleCount/4);
                unsigned char byte_R = (unsigned char)((tempR/sampleCount)/4);
                //Create G
                unsigned short tempG = 0;
                unsigned short verticalB = 0;
                unsigned short horizontalB = 0;
                int includeBN = 1;
                int includeBE = 1;
                int includeBW = 1;
                int includeBS = 1;
                if(y-2 >= 0 && y+2 < height && x-2 >= 0 && x+2 < width) {
                    verticalB = abs(data[(y-2)*width + x] - data[(y+2)*width + x]);
                    horizontalB = abs(data[y*width + x -2] - data[y*width + x + 2]);
                    if(verticalB > horizontalB) {
                        includeBN = 0;
                        includeBS = 0;
                    } else if (verticalB < horizontalB) {
                        includeBE = 0;
                        includeBW = 0;
                    }
                }

                sampleCount = 0;
                if( y-1 >= 0 && includeBN == 1)
                {
                    tempG += data[(y-1)*width + x];
                    sampleCount++;
                }
                if(x-1 >= 0 && includeBW == 1)
                {
                    tempG += data[y*width + (x-1)];
                    sampleCount++;
                }
                if(x+1 < width && includeBE == 1)
                {
                    tempG += data[y*width + (x+1)];
                    sampleCount++;
                }
                if(y+1 < height && includeBS == 1)
                {
                    tempG += data[(y+1)*width + x];
                    sampleCount++;
                }
                //unsigned char byte_G = (unsigned char)(tempG/sampleCount/4);
                unsigned char byte_G = (unsigned char)((tempG/sampleCount)/4);
                
                //Create B
                unsigned char byte_B = (unsigned char)((data[y*width + x])/4);
                
                color[0] = byte_R;
                color[1] = byte_G;
                color[2] = byte_B;
            }
            else
            {
                printf("Unknown type %d %d\n", y, x);
            }

            totalR += color[0];
            totalG += color[1];
            totalB += color[2];

            rgb[index] = color[0];
            index++;
            rgb[index] = color[1];
            index++;
            rgb[index] = color[2];
            index++;
        }
    }
}

