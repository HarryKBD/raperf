//
//  main.c
//  Converter
//
//  Created by kapsu han on 19/10/2016.
//  Copyright © 2016 Kapsu Han. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <math.h>

int width = 1920;
int height = 1920;
int calW = 1920;
int calH = 1920;
int alignedW = 1920;
int alignedH = 1920;
char * inputFile;
char * outputFile;
char * gOutput;
char * pngOutput;
int size = 4608000;

int autoMode = 0;
int bayerFormat = 0; //0: RGGB 1: GRBG
int balance = 255;

int balanceR = 255;
int balanceG = 255;
int balanceB = 255;
int kelvin = 0;
int contrast = 0; // -255 to 255
int brightness = 0;// -255 to 255

int histogramMin = 0;
int histogramMax = 0;
float gammaCorrection = 1.0f;
float expVal = 0.0f;
int histogramVal = 5;
int histogramEqualisation = 0;
int blackMax = 0;
int sharpen = 1;
long int totalR = 0;
long int totalG = 0;
long int totalB = 0;
typedef struct {
    int id;
    char *command;
    char *abbrev;
    int num_parameters;
} COMMAND_LIST;
#define CommandWidth 1
#define CommandHeight 2
#define CommandAlignedWidth 3
#define CommandAlignedHeight 4
#define CommandCalWidth 5
#define CommandCalHeight 6
#define CommandInputfile 7
#define CommandBufferSize 8
#define CommandOutput 9
#define CommandBayerFormat 10
#define CommandBalance 11
#define CommandBalanceR 12
#define CommandBalanceG 13
#define CommandBalanceB 14
#define CommandContrast 15
#define CommandBrightness 16
#define CommandHistogramMin 17
#define CommandHistogramMax 18
#define CommandGammaCorrection 19
#define CommandHistogramVal 20
#define CommandBlackAboveMax 21
#define CommandGreyOutput 22
#define CommandPngOutput 23
#define CommandSharpen 24
#define CommandHistogramEqualisation 25
#define CommandExposure 26
#define CommandAutoMode 27
static COMMAND_LIST cmdline_commands[] =
{
    {CommandWidth, "-width", "w", 1},
    {CommandHeight, "-height", "h", 1},
    {CommandAlignedWidth, "-shapedwidth", "aw", 1},
    {CommandAlignedHeight, "-shapedheight", "ah", 1},
    {CommandCalWidth, "-calculatedwidth", "cw", 1},
    {CommandCalHeight, "-calculatedheight", "ch", 1},
    {CommandInputfile, "-input", "i", 1},
    {CommandBufferSize, "-size", "s", 1},
    {CommandOutput, "-out", "o", 1},
    {CommandBayerFormat, "-format", "f", 1},
    {CommandBalance, "-balance", "bl", 1},
    {CommandBalanceR, "-balancer", "br", 1},
    {CommandBalanceG, "-balanceg", "bg", 1},
    {CommandBalanceB, "-balanceb", "bb", 1},
    {CommandContrast, "-contrast", "c", 1},
    {CommandBrightness, "-brightness", "b", 1},
    {CommandHistogramMin, "histogrammin", "hmin", 1},
    {CommandHistogramMax, "histogrammax", "hmax", 1},
    {CommandGammaCorrection, "gammacorrection", "gc", 1},
    {CommandHistogramVal, "histogramValue", "v", 1},
    {CommandBlackAboveMax, "BlackMax", "bm", 0},
    {CommandGreyOutput, "GreyOut", "go", 1},
    {CommandPngOutput, "PngOut", "po", 1},
    {CommandSharpen, "-sharp", "sp", 0},
    {CommandHistogramEqualisation, "-histogramequalisation", "he", 0},
    {CommandExposure, "-exposure", "e", 1},
    {CommandAutoMode, "-auto", "a", 0}
};

int getCommandId(const COMMAND_LIST *commands, const int num_commands, const char *arg, int *num_parameters)
{
    int command_id = -1;
    int j;
    
    if(!commands || !num_parameters || !arg)
        return -1;
    
    for(j=0 ; j < num_commands ; j++)
    {
        if(!strcmp(arg, commands[j].abbrev)) {
            command_id = commands[j].id;
            *num_parameters = commands[j].num_parameters;
            break;
        }
    }
    return command_id;
}
float clampMinMaxFloat(float val, float min, float max)
{
    if(val < min)
        return min;
    if(val > max)
        return max;
    return val;
}
int clampMinMax(int val, int min, int max)
{
    if(val < min)
        return min;
    if(val > max)
        return max;
    return val;
}
static int cmdline_commands_size = sizeof(cmdline_commands)/sizeof(cmdline_commands[0]);
static int parse_cmdLine(int argc, const char **argv)
{
    int i ;
    int valid = 1;
    for(i = 1 ; i < argc ; i++)
    {
        int num_parameters;
        if(!argv[i])
            continue;
        if(argv[i][0] != '-')
        {
            valid = 0;
            continue;
        }
        int command_id = getCommandId(cmdline_commands, cmdline_commands_size, &argv[i][1], &num_parameters);
        
        switch(command_id)
        {
            case CommandInputfile:
            {
                int len = strlen(argv[i + 1]);
                if(len)
                {
                    inputFile = (char*)malloc(len+1);
                    if(inputFile)
                    {
                        strncpy(inputFile, argv[i+1], len+1);
                    }
                    i++;
                }
                else
                {
                    valid = 0;
                }
                break;
            }
            case CommandOutput:
            {
                int len = strlen(argv[i + 1]);
                if(len)
                {
                    outputFile = (char*)malloc(len+1);
                    if(inputFile)
                    {
                        strncpy(outputFile, argv[i+1], len+1);
                    }
                    i++;
                }
                else
                {
                    valid = 0;
                }
                break;
            }
             case CommandGreyOutput:
            {
                int len = strlen(argv[i + 1]);
                if(len)
                {
                    gOutput = (char*)malloc(len+1);
                    if(inputFile)
                    {
                        strncpy(gOutput, argv[i+1], len+1);
                    }
                    i++;
                }
                else
                {
                    valid = 0;
                }
                break;
            }   
             case CommandPngOutput:
            {
                int len = strlen(argv[i + 1]);
                if(len)
                {
                    pngOutput = (char*)malloc(len+1);
                    if(inputFile)
                    {
                        strncpy(pngOutput, argv[i+1], len+1);
                    }
                    i++;
                }
                else
                {
                    valid = 0;
                }
                break;
            }   

            case CommandWidth:
            {
                if(sscanf(argv[i+1], "%u", &width) != 1) {
                    valid = 0;
                }
                else
                    i++;
                break;
            }
            case CommandHeight:
            {
                if(sscanf(argv[i+1], "%u", &height) != 1) {
                    valid = 0;
                }
                else
                    i++;
                printf("Height %d\n", height);
                break;
            }
            case CommandAlignedWidth:
            {
                if(sscanf(argv[i+1], "%u", &alignedW) != 1) {
                    valid = 0;
                }
                else
                    i++;
                printf("alignedW %d\n", alignedW);
                break;
            }
            case CommandAlignedHeight:
            {
                if(sscanf(argv[i+1], "%u", &alignedH) != 1) {
                    valid = 0;
                }
                else
                    i++;
                printf("alignedH %d\n", alignedH);
                break;
            }
            case CommandCalWidth:
            {
                if(sscanf(argv[i+1], "%u", &calW) != 1) {
                    valid = 0;
                }
                else
                    i++;
                printf("CalWidth %d\n", calW);

                break;
            }
            case CommandCalHeight:
            {
                if(sscanf(argv[i+1], "%u", &calH) != 1) {
                    valid = 0;
                }
                else
                    i++;
                printf("CalHeight %d\n", calH);
                break;
            }
                
            case CommandBufferSize:
            {
                if(sscanf(argv[i+1], "%u", &size) != 1) {
                    valid = 0;
                }
                else
                    i++;
                break;
            }
            case CommandBayerFormat:
            {
                if(sscanf(argv[i+1], "%u", &bayerFormat) != 1) {
                    valid = 0;
                }
                else
                    i++;
                break;
            }
            case CommandBalance:
            {
                if(sscanf(argv[i+1], "%u", &balance) != 1) {
                    valid = 0;
                }
                else
                    i++;
                break;
            }
            case CommandBalanceR:
            {
                if(sscanf(argv[i+1], "%u", &balanceR) != 1) {
                    valid = 0;
                }
                else
                    i++;
                break;
            }
            case CommandBalanceG:
            {
                if(sscanf(argv[i+1], "%u", &balanceG) != 1) {
                    valid = 0;
                }
                else
                    i++;
                break;
            }
            case CommandBalanceB:
            {
                if(sscanf(argv[i+1], "%u", &balanceB) != 1) {
                    valid = 0;
                }
                else
                    i++;
                break;
            }
            case CommandContrast:
            {
                if(sscanf(argv[i+1], "%u", &contrast) != 1) {
                    valid = 0;
                }
                else
                    i++;
                contrast = clampMinMax(contrast, -255, 255);
                printf("Contrast set to %d\n", contrast);
                break;

            }
            case CommandBrightness:
            {
                if(sscanf(argv[i+1], "%u", &brightness) != 1) {
                    valid = 0;
                }
                else
                    i++;
                brightness = clampMinMax(brightness, -255, 255);
                printf("Brightness set to %d\n", brightness);
                break;
            }
            case CommandHistogramMin:
            {
                if(sscanf(argv[i+1], "%u", &histogramMin) != 1) {
                    valid = 0;
                }
                else
                    i++;
                histogramMin = clampMinMax(histogramMin, 0, 255);
                printf("Histogram min set to %d\n", histogramMin);
                break;
            }
            case CommandHistogramVal:
            {
                if(sscanf(argv[i+1], "%u", &histogramVal) != 1) {
                    valid = 0;
                }
                else
                    i++;
                printf("Histogram val set to %d\n", histogramVal);
                break;
            }
            case CommandHistogramMax:
            {
                if(sscanf(argv[i+1], "%u", &histogramMax) != 1) {
                    valid = 0;
                }
                else
                    i++;
                histogramMax = clampMinMax(histogramMax, 0, 255);
                printf("Histogram max set to %d\n", histogramMax);
                break;
            }
            case CommandGammaCorrection:
            {
                if(sscanf(argv[i+1], "%f", &gammaCorrection) != 1) {
                    valid = 0;
                }
                else
                    i++;
                gammaCorrection = clampMinMaxFloat(gammaCorrection, 0.01, 7.99);
                printf("Gamma Correction set to %f\n", gammaCorrection);
                break;
            }
            case CommandExposure:
            {
                if(sscanf(argv[i+1], "%f", &expVal) != 1) {
                    valid = 0;
                }
                else
                    i++;
                printf("Exposure Value set to %f\n", expVal);
                break;
            }
            case CommandBlackAboveMax:
            {
                blackMax = 1;
                printf("Black max enabled %d\n", blackMax);
                break;
            }
            case CommandSharpen:
            {
                sharpen = 1;
                printf("Sharpening enabled %d\n", sharpen);
                break;
            }
            case CommandHistogramEqualisation:
            {
                histogramEqualisation = 1;
                printf("Histogram Equalisation enabled %d\n", histogramEqualisation);
                break;
            }
            case CommandAutoMode:
            {
                autoMode = 1;
                printf("AutoMode enabled %d]n", autoMode);
                break;
            }
        }
    }
    
    return 1;
}
int clamp(int color)
{
    if(color < 0)
        return 0;
    if(color > 255)
        return 255;
    return color;
}
unsigned char exposure(unsigned char color)
{
    int newColor = (int)(color*pow(2, (expVal/2)));
    return (unsigned char)clamp(newColor);
}
void applyExposure(unsigned char * data)
{
    int index = 0;
    for(index = 0 ; (index+2) < (height*width*3) ; index +=3)
    {
        data[index] = exposure(data[index]);
        data[index+1] = exposure(data[index+1]);
        data[index+2] = exposure(data[index+2]);
    
    }
}
unsigned char processGammaCorrection(unsigned char color)
{
    float inputcolor = (float)color/255;
    float gammVal = 1/gammaCorrection;
    int newColor = (int)(255*(pow(inputcolor, gammVal)));
    return (unsigned char)clamp(newColor);
}
void applyGammaCorrection(unsigned char * data)
{
    int index = 0;
    for(index = 0 ; (index+2) < (height*width*3) ; index +=3)
    {
        data[index] = processGammaCorrection(data[index]);
        data[index+1] = processGammaCorrection(data[index+1]);
        data[index+2] = processGammaCorrection(data[index+2]);
    }
}
unsigned char processHistogram(unsigned char color)
{
    int newcolor = (int)(((float)color-(float)histogramMin)/((float)histogramMax-(float)histogramMin)*255);
    return (unsigned char)clamp(newcolor);
}
void applyHistogram(unsigned char * data)
{
    int index = 0;
    for(index = 0 ; (index+2) < (height*width*3) ; index +=3)
    {
/*
        if((int)( 0.2989*data[index] + 0.5870*data[index+1] + 0.1140*data[index+2]) > histogramMax) {
            data[index] = 0;
            data[index+1] = 0;
            data[index+2] = 0;
        } else {
*/
            data[index] = processHistogram(data[index]);
            data[index+1] = processHistogram(data[index+1]);
            data[index+2] = processHistogram(data[index+2]);
//       }
    }
}
unsigned char applyBrightnessToColor(unsigned char color)
{
    int newcolor = color+(brightness/2);
    return (unsigned char)clamp(newcolor);
}
void applyBrightness(unsigned char * data)
{
    int index = 0;
    for(index = 0 ; (index+2) < (height*width*3) ; index +=3)
    {
        data[index] = applyBrightnessToColor(data[index]);
        data[index+1] = applyBrightnessToColor(data[index+1]);
        data[index+2] = applyBrightnessToColor(data[index+2]);
    }
}
unsigned char contrastToColor(unsigned char color)
{
    //printf("ContrastToColor %i %f\n", (int)color, (float)color);
    float new_v = (float)color/255;
    //printf("ContrastToColor %f\n", new_v);
    new_v -= 0.5;
    
    new_v *= (contrast/100);
    new_v += 0.5;
    new_v *= 255;
    return (unsigned char)clamp(new_v);
}
void applyContrastFactor(unsigned char * data)
{
    contrast /= 2;
    float contrastFactor = (259*((float)contrast+255))/(255*(259-(float)contrast));
    printf("Contrast Factor %f\n", contrastFactor);
    int index = 0;
    for(index = 0 ; (index+2) < (height*width*3) ; index +=3)
    {
        data[index] = clamp(contrastFactor*(data[index]-128) + 128);
        data[index+1] = clamp(contrastFactor*(data[index+1]-128) + 128);
        data[index+2] = clamp(contrastFactor*(data[index+2]-128) + 128);
    }
}

unsigned char withinRange(unsigned char color, int balance)
{
    int newColor = ((int)color) * 255/balance;
    
    if(newColor > 255)
    {
        //printf("> 255\n");
        return 255;
    }
    else if(newColor < 0)
    {
        //printf("< 0\n");
        return 0;
    }
    return (unsigned char)newColor;
}

void filterPureWhite(unsigned char * data)
{
    int index = 0;
    for(index = 0 ; (index+2) < (height*width*3) ; index +=3)
    {
        if(data[index] == 0xFF && data[index+1] == 0xFF && data[index+2] == 0xFF)
        {
            data[index] = 0x00;
            data[index+1] = 0x00;
            data[index+2] = 0x00;
        }
        
    }
}
void applyBalance(unsigned char * data)
{
    int index = 0;
    for(index = 0 ; (index+2) < (height*width*3) ; index +=3)
    {
        if(balanceR < 255)
            data[index] = withinRange(data[index], balanceR);
        if(balanceG < 255)
            data[index+1] = withinRange(data[index+1], balanceG);
        if(balanceB < 255)
            data[index+2] = withinRange(data[index+2], balanceB);
    }
}

void convertGRBG(unsigned short * data, unsigned char * rgb)
{
    int x = 0, y = 0;
    int index = 0;
    for(y = 0 ; y < height ; y++)
    {
        for( x = 0 ; x < width ; x++ )
        {
            static unsigned char color[3];
            //printf("Interpolation %d %d ", y, x);
            if(y%2 == 0 && x%2 == 0)
            {
                
                //G1 pixel
                
                
                //create R
                unsigned short tempR = 0;
                int sampleCount = 0;
                if(x-1 >= 0)
                {
                    tempR += data[y*width + x - 1];
                    sampleCount++;
                }
                if(x+1 < width)
                {
                    tempR += data[y*width + x + 1];
                    sampleCount++;
                }
                
                unsigned char byte_R = (unsigned char)(tempR/sampleCount/4);
                
                unsigned char byte_G = (unsigned char)(data[y*width + x]/4);
                
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
                unsigned char byte_B = (unsigned char)(tempB/sampleCount/4);
                color[0] = byte_R;
                color[1] = byte_G;
                color[2] = byte_B;
                
                //(void)fwrite(color, 1, 3, out);
            }
            else if(y%2 == 0 && x%2 == 1)
            {
                //R pixel
                //printf("G1\n");
                //create R
                int sampleCount = 0;
                
                unsigned char byte_R = (unsigned char)(data[y*width + x]/4);
                
                //Create G
                unsigned short tempG = 0;
                sampleCount = 0;
                if(y-1 >= 0)
                {
                    tempG += data[(y-1)*width + x];
                    sampleCount++;
                }
                if(x-1 >= 0)
                {
                    tempG += data[y*width + x -1];
                    sampleCount++;
                }
                if(x+1 < width)
                {
                    tempG += data[y*width + x + 1];
                    sampleCount++;
                }
                if(y+1 < height)
                {
                    tempG += data[(y+1)*width + x];
                    sampleCount++;
                }
                unsigned char byte_G = (unsigned char)(tempG/sampleCount/4);
                
                
                //Create B
                unsigned short tempB = 0;
                sampleCount = 0;
                if(y-1 >= 0 && x-1 >= 0)
                {
                    tempB += data[(y-1)*width + x - 1];
                    sampleCount++;
                }
                if(y-1 >= 0 && x+1 < width)
                {
                    tempB += data[(y-1)*width + x + 1];
                    sampleCount++;
                }
                if(y+1 < height && x-1 >= 0)
                {
                    tempB += data[(y+1)*width + x - 1];
                    sampleCount++;
                }
                if(y+1 < height && x+1 < width)
                {
                    tempB += data[(y+1)*width + x + 1];
                    sampleCount++;
                }
                unsigned char byte_B = (unsigned char)((tempB/sampleCount)/4);
                
                color[0] = byte_R;
                color[1] = byte_G;
                color[2] = byte_B;
                //(void)fwrite(color, 1, 3, out);
                
            }
            else if(y%2 == 1 && x%2 == 0)
            {
                //B pixel
                
                //create R
                unsigned short tempR = 0;
                int sampleCount = 0;
                sampleCount = 0;
                if(y-1 >= 0 && x-1 >= 0)
                {
                    tempR += data[(y-1)*width + x -1];
                    sampleCount++;
                }
                if(y-1 >= 0 && x+1 < width)
                {
                    tempR += data[(y-1)*width + x + 1];
                    sampleCount++;
                }
                if(y+1 < height && x-1 >= 0)
                {
                    tempR += data[(y+1)*width + x - 1];
                    sampleCount++;
                }
                if(y+1 < height && x+1 < width)
                {
                    tempR += data[(y+1)*width + x + 1];
                    sampleCount++;
                }

                //unsigned char byte_R = (unsigned char)(tempR/sampleCount/4);
                unsigned char byte_R = (unsigned char)((tempR/sampleCount)/4);
                
                //Create G
                unsigned short tempG = 0;
                sampleCount = 0;
                if(y-1 >= 0)
                {
                    tempG += data[(y-1)*width + x];
                    sampleCount++;
                }
                if(x-1 >= 0)
                {
                    tempG += data[y*width + x -1];
                    sampleCount++;
                }
                if(x+1 < width)
                {
                    tempG += data[y*width + x + 1];
                    sampleCount++;
                }
                if(y+1 < height)
                {
                    tempG += data[(y+1)*width + x];
                    sampleCount++;
                }
                unsigned char byte_G = (unsigned char)(tempG/sampleCount/4);
                
                //Create B
                unsigned char byte_B = (unsigned char)(data[y*width + x]/4);
                
                color[0] = byte_R;
                color[1] = byte_G;
                color[2] = byte_B;
                
            }
            else if(y%2 == 1 && x%2 == 1)
            {
                //G2 pixel
                
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
                unsigned char byte_R = (unsigned char)((tempR/sampleCount)/4);
                
                
                //Create G
                unsigned char byte_G = (unsigned char)(data[y*width + x]/4);
                
                //Create B
                unsigned short tempB = 0;
                sampleCount++;
                if(x-1 >= 0)
                {
                    tempB += data[y*width + x - 1];
                    sampleCount++;
                }
                if(x+1 < width)
                {
                    tempB += data[y*width + x + 1];
                    sampleCount++;
                }
                unsigned char byte_B = (unsigned char)(tempB/sampleCount/4);
                
                color[0] = byte_R;
                color[1] = byte_G;
                color[2] = byte_B;
            }
            else
            {
                printf("Unknown type %d %d\n", y, x);
            }

            rgb[index] = color[0];
            index++;
            rgb[index] = color[1];
            index++;
            rgb[index] = color[2];
            index++;
        }
    }
}
void convertRGGB(unsigned short * data, unsigned char * rgb)
{
    int x = 0, y = 0;
    int index = 0;
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
    printf("Total R %ld G %ld B %ld\n", totalR, totalG, totalB);
}

void applyWeight(unsigned char * data)
{
    float weightGreen = 0;
    float weightRed = 0;
    float weightBlue = 0;
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
double sRGB_to_linear(double x)
{
    if(x < 0.04045) return x/12.92;
    return pow((x+0.055)/1.055, 2.4);
}

int main(int argc, const char * argv[]) {
    // insert code here...
    printf("Hello, World!\n");
    parse_cmdLine(argc, argv);
    printf("Cmd %d %d %d %d %d %d %s\n", width, height, alignedW, alignedH, calW, calH, inputFile);
    int fd;
    //int out;
    
    // Define 10 bit array
    int arrayW = size/alignedH;
    int arrayH = alignedH;
    printf("Array %d %d\n", arrayW, arrayH);
    unsigned char *data;
    data = (unsigned char *)malloc(sizeof(unsigned char) * arrayW * arrayH);
    int len;
    fd = open(inputFile, O_RDONLY);

    //out = creat(outputFile, O_RDWR);
    if(fd < 0) {
        perror("open");
        free(data);
        
    }
    
    //Read input file
    len = read(fd, data, size);
    if(len != size)
    {
        printf("Read tried %d got %d\n", size, len);
    }
    else
    {
        printf("Read ok\n");
    }
    close(fd);
    
    
    int shapedW = arrayW;
/*
    if(alignedW > calW)
    {
        shapedW = arrayW - 24;
    }
*/
    int shapedH = height;
    
    printf("Shaped h = %d, w = %d\n", shapedH, shapedW);
    int row = 0;
    int col = 0;
    printf("Size of 10bit array %d %d\n", height, width);
    
    //Allocation 16bit memory
    unsigned short * data2;
    data2 = (unsigned short *)malloc(sizeof(unsigned short) * width * height);
    
    int x;
    int y = 0;
    
    /**********************************************
     Convert 5 1byte data
     AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD aabbccdd
     
     ==> 4 2byte data
     000000AAAAAAAAaa 000000BBBBBBBBbb 000000CCCCCCCCcc 000000DDDDDDDDdd
     **********************************************/


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
            //printf("Hi %d, %d, %d, %d\n", byte1/4, byte2/4, byte3/4, byte4/4);
//            map[byte1/4]++;
//            map[byte2/4]++;
//            map[byte3/4]++;
//            map[byte4/4]++;

//            printf("10bit %u %u %u %u %u to %hx %hx %hx %hx\n", data[row][col],
//                   data[row][col+1],
//                   data[row][col+2],
//                   data[row][col+3],
//                   data[row][col+4],
//                   byte1, byte2, byte3, byte4);
            
            x+=4;
            
        }
        y++;
    }

    free(data);
        //Get Histogram map

    //Allocate memory buffer with size of wxhx3(RGB)
    unsigned char * interpolatedData;
    interpolatedData = (unsigned char*)malloc(sizeof(unsigned char)*height*width*3);
    
    //Check bayer format type and call corresponding interpolator.
    if(bayerFormat == 0)
    {
        convertRGGB(data2, interpolatedData);
    }
    else if(bayerFormat == 1)
    {
        convertGRBG(data2, interpolatedData);
    }
    else
    {
        printf("Unknow bayerformat so will take RGGB format\n");
        convertRGGB(data2, interpolatedData);
    }

    /****************
     Apply colour weight according to total sum of each colour
     Green is normally the highest
     ****************/
    applyWeight(interpolatedData);
    

    //filterPureWhite(interpolatedData);
    
    /****************
     Apply balance for each colour if any balance given below 255
     ****************/
    if(balanceR < 255 || balanceG < 255 || balanceB < 255) {
        printf("Apply colour balance R:%d G:%d B:%d\n", balanceR, balanceG, balanceB);
        applyBalance(interpolatedData);
    }
    

    if(autoMode)
    {
        int colorMapIndex = 0;
        int totalColorSum = 0;
        int colorMap[256] = {0};
        int bufferIndex = 0;
        printf("Colllection 4 area data\n");
        for(bufferIndex = 0 ; (bufferIndex+2) < (height*width*3) ; bufferIndex +=3)
        {
            int mapIndex = (int)( 0.2989*interpolatedData[bufferIndex] + 0.5870*interpolatedData[bufferIndex+1] + 0.1141*interpolatedData[bufferIndex+2]);
            if(mapIndex > 20 && mapIndex < 240) {
                colorMap[mapIndex]++;
                totalColorSum++;
            }
            
        }

        int map4[4] = {0};
        for(colorMapIndex = 0 ; colorMapIndex < 256 ; colorMapIndex++)
        {
            map4[(colorMapIndex+1)/64] += colorMap[colorMapIndex];
        }
        int map4Index = 0;
        for(map4Index = 0 ; map4Index < 4 ; map4Index++) {
            printf("4 Area [%i] = %i %f percent\n", map4Index, map4[map4Index], (float)map4[map4Index]/totalColorSum*100);
        }

        float darkAreaPercentage = (float)map4[0]/totalColorSum*100;
        float brightkAreaPercentage = (float)map4[2]/totalColorSum*100;
        printf("Dark area percentage is %f\n", darkAreaPercentage);
        if(darkAreaPercentage > 80 && brightkAreaPercentage < 5)
        {
            printf("ex = 4 gc 0.7 b 80 contrast 100\n");
            expVal = 4;
            gammaCorrection = 0.7;
            brightness = 80;
            contrast = 100;
        }
        else if(darkAreaPercentage > 60)
        {
            printf("b 50 contrast 100\n");
            expVal = 1;
            gammaCorrection = 1;
            brightness = 50;
            contrast = 100;
        }
        else
        {
            printf("No change\n");
            expVal = 1;
            gammaCorrection = 1;
        }
    }
    
    if(expVal != 0.0)
    {
        applyExposure(interpolatedData);
    }

    /****************
     Apply Gamma correction
     ****************/
    
    if(gammaCorrection > 0.0 && gammaCorrection < 8.0 && gammaCorrection != 1.0)
    {
        applyGammaCorrection(interpolatedData);
    }
    /*******************
     Calculate histogram map
     *********************/
    if(histogramMin != 0 || histogramMax != 0)
    {
    
        int index = 0;
        int totalSum = 0;
        int map[256] = {0};
        int perMap[256] = {0};
        printf("Hellow\n");
        for(index = 0 ; (index+2) < (height*width*3) ; index +=3)
        {
            int mapIndex = (int)( 0.2989*interpolatedData[index] + 0.5870*interpolatedData[index+1] + 0.1141*interpolatedData[index+2]);

            if(mapIndex > 20 && mapIndex < 240) {
                map[mapIndex]++;
                totalSum++;
            }

        }

        int tempSum = 0;
        for(index = 0 ; index <= 0xFF ; index++)
        {
            tempSum += map[index];
            int per = (int)((float)tempSum*10000/totalSum);
            //printf("MAP[%d] = %d %i percent\n", index, map[index], per);
            perMap[index] = per;
            
        }
        int threasholdMin = histogramMin;
        int minFound = 0;
        int threasholdMax = histogramMax;
        for(index = 0 ; index < 0xFF ; index++)
        {
            if(minFound == 0 && perMap[index] <= threasholdMin) {
                histogramMin = index;
                minFound = 1;
            }
            if(perMap[index] <= 10000-threasholdMax) {
                histogramMax = index;
            }
        }
        printf("Histogran min %i max %d\n", histogramMin, histogramMax);
        /****************
         Apply Histogram stretching
         ****************/


        if(histogramMin > 0 || histogramMax < 255)
        {
            printf("Appy Histogram stretching min %d max %d\n", histogramMin, histogramMax);
            applyHistogram(interpolatedData);
        }
    }

    /****************
     Apply Brightness if brightenss value is not zero
     ****************/
    
    if(brightness != 0)
    {
        printf("Apply brightness %d\n", brightness);
        applyBrightness(interpolatedData);
    }
    
    
    /****************
     Apply Contrast if brightenss value is not zero
     ****************/
    
    if(contrast != 0)
    {
        printf("Apply contrast %d\n", contrast);
        applyContrastFactor(interpolatedData);
    }

    /****************
     Write to output file with ppm header
     ****************/


    FILE * out;
    out = fopen(outputFile, "wb");
    if(out < 0) {
        perror("open");
        return -1;
    }
    // PPM Header
    int headerSize = 0;
    headerSize = fprintf(out, "P6\n%d %d\n255\n", width, height);
    printf("Header Size is %d", headerSize);
    
    //Write interpolatedData to output
    if(fwrite(interpolatedData, 1, sizeof(unsigned char)*height*width*3, out) != sizeof(unsigned char)*height*width*3)
    {
        printf("Wrong at writing\n");
    }
    fclose(out);

    free(interpolatedData);
    printf("Finished\n");
    
    return 0;
}

