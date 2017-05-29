/*
 Copyright (c) 2015, Raspberry Pi Foundation
 Copyright (c) 2015, Dave Stevenson
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of the copyright holder nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


//#include <wiringPi.h>
//#include <wiringPiI2C.h>
#include <linux/i2c-dev.h>
#include <sys/mman.h>
#include "bcm_host.h"
#include "interface/vcos/vcos.h"
#include "bcm_host.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_connection.h"

#include <sys/ioctl.h>
#include "RaspiCLI.h"

int compRatio = 0;
// Do the GPIO waggling from here, except that needs root access, plus
// there is variation on pin allocation between the various Pi platforms.
// Provided for reference, but needs tweaking to be useful.
//#define DO_PIN_CONFIG
/// GPIO controll
#define BCM2708_PERI_BASE	0x3F000000
#define GPIO_BASE		(BCM2708_PERI_BASE + 0x200000)
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

#define GPIO_CAMERA_RECORDING_OPERATION 2
#define GPIO_CAMERA_OPERATION 3
int mem_fd;
void *gpio_map;
volatile unsigned * gpio;

#define INP_GPIO(g) *(gpio+((g)/10)) &=~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |= (1<<(((g)%10)*3))
#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock
void setup_io();
void changeStatusOfCam(int on);
void changeStatusOfRecording(int recording);

void* flux_memcpy(void* dest, const void* src, size_t count);
void* flux_memcpy2(void* dest, const void* src, size_t count);
void* flux_memcpy3(void* dest, const void* src, size_t count);
struct sensor_regs {
    uint16_t reg;
    uint8_t  data;
};
uint64_t prev_time = 0;
//#define WIDTH 3280 //2592
//#define HEIGHT 2464 //1944
#define WIDTH 3280
#define HEIGHT  2464

int use_write_thread = 1;
//#define RAW16
//#define RAW8

#ifdef RAW16
#define ENCODING MMAL_ENCODING_BAYER_SBGGR16
#define UNPACK MMAL_CAMERA_RX_CONFIG_UNPACK_10;
#define PACK MMAL_CAMERA_RX_CONFIG_PACK_16;
#elif defined RAW8
#define ENCODING MMAL_ENCODING_BAYER_SBGGR8
#define UNPACK MMAL_CAMERA_RX_CONFIG_UNPACK_10;
#define PACK MMAL_CAMERA_RX_CONFIG_PACK_8;
#else
#define ENCODING MMAL_ENCODING_BAYER_SBGGR10P
#define UNPACK MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
#define PACK MMAL_CAMERA_RX_CONFIG_PACK_NONE;
#endif
#define CSI_IMAGE_ID 0x2B
#define CSI_DATA_LANES 2
// Max bitrate we allow for recording
const int MAX_BITRATE_MJPEG = 25000000; // 25Mbits/s
const int MAX_BITRATE_LEVEL4 = 25000000; // 25Mbits/s
const int MAX_BITRATE_LEVEL42 = 62500000; // 62.5Mbits/s
int buffer_size = 0;
int64_t previous_frame_time = 0;
int writing_file_started = 0;
int waiting_for_frame_buffer = 0;
int buffer_delivered = 0;

uint64_t record_start_pressed_time = 0;
int frame_step = 1;
int width = WIDTH;
int height = HEIGHT;
int x_start = 0;
int x_end = WIDTH - 1;
int y_start = 0;
int y_end = HEIGHT- 1;
int hts = 5785;
int vts = 3816;
int exposure = 0;
int previewMode = 0;
int node_count = 0;
int format = 0;
int encodingBit = 8;
void set_size_v2(void);

MMAL_POOL_T *pool;
/** Struct to keep frame buffers in RAM and other thread will write sequentially.
 */
struct FLUX_BUFFERNODE
{
    FILE * file_handle;
    uint8_t *data;
    uint32_t length;
    int index;
    struct FLUX_BUFFERNODE * next;
};

int write_thread_create = 0;
int write_thread_started = 0;
int pre_alloc = 0;
int allocateNode(int buffer_size);
void add_buffer_node(FILE * file_handle, MMAL_BUFFER_HEADER_T *buffer, MMAL_PORT_T *port);
void add_buffer_to_node_chain(FILE * file_handle, MMAL_BUFFER_HEADER_T *buffer, MMAL_PORT_T *port);
void add_buffer_to_node_chain_devided(FILE * file_handle, MMAL_BUFFER_HEADER_T *buffer, MMAL_PORT_T *port);
void write_first_node();
struct FLUX_BUFFERNODE * get_front_buffer_node();
struct FLUX_BUFFERNODE * firstBufferNode = NULL;
struct FLUX_BUFFERNODE * nextBufferNode = NULL;
struct FLUX_BUFFERNODE * lastBufferNode = NULL;
struct FLUX_BUFFERNODE * lastfreedNode = NULL;
struct FLUX_BUFFERNODE * firstfreedNode = NULL;
void * write_queued_buffer(void);
void * write_one_queued_buffer(void);
pthread_cond_t condAvail = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int buffer_queue_index = 0;
FILE * ofile;
FILE * dumfile;
FILE * pFile;
int dfile;
int initial_frames_to_skip = 10;

void display_supported_encodings(MMAL_PORT_T *port);
// These register settings were as logged off the line
// by jbeale. There is a datasheet for OV5647 floating
// about on the internet, but the Pi Foundation have
// information from Omnivision under NDA, therefore
// we can not offer support on this.
// There is some information/discussion on the Freescale
// i.MX6 forums about supporting OV5647 on that board.
// There may be information available there that is of use.
//
// REQUESTS FOR SUPPORT ABOUT THESE REGISTER VALUES WILL
// BE IGNORED.
/**
 * IMX219 sensor reg
 */
#define SLAVE_ADDR 0x20 >> 1
//#define SLAVE_ADDR 0x64
#define ES_GAIN(a,b,c) ((unsigned short)(a*160)<(c*10) && (c*10)<=(unsigned short)(b*160))
struct sensor_regs imx219_start1[] = { //3280 * 2464 20fps 4lane
    {0x30EB, 0x05},
    {0x30EB, 0x0C},
    {0x300A, 0xFF},
    {0x300B, 0xFF},
    {0x30EB, 0x05},
    {0x30EB, 0x09},
    
    {0x0114, 0x01}, //CSI_LANE_MODE 1: 2Lane 3: 4Lane
    {0x0128, 0x00},
    {0x012A, 0x13},
    {0x012B, 0x34}
};
const int NUM_CAP_V2REGS_START1 = sizeof(imx219_start1) / sizeof(struct sensor_regs);
struct sensor_regs imx219_default_fps_regs[] = {
    //97ms Success
    //hts
    {0x0160, 0x16}, //5632
    {0x0161, 0x00},
    //vts
    {0x0162, 0x0E}, //3816
    {0x0163, 0xE8}
    
    //OK
    //       {0x0160, 0x03}, //1000
    //       {0x0161, 0xE8},
    
    //       {0x0160, 0x0B}, //2000
    //       {0x0161, 0xB8},
    
    //       {0x0162, 0x0D}, //3448
    //       {0x0163, 0x78}
};
const int NUM_CAP_V2REGS_DEF_FPS = sizeof(imx219_default_fps_regs) / sizeof(struct sensor_regs);
struct sensor_regs imx219_default_size_regs[] = {
    //2592x1944
    {0x0164, 0x00}, //0
    {0x0165, 0x00},
    {0x0166, 0x0A}, //2591
    {0x0167, 0x1F},
    {0x0168, 0x00}, //0
    {0x0169, 0x00},
    {0x016A, 0x07}, //1943
    {0x016B, 0x97},
    {0x016C, 0x0A}, //2592
    {0x016D, 0x20},
    {0x016E, 0x07}, //1944
    {0x016F, 0x98}
};
const int NUM_CAP_V2REGS_DEF_SIZE = sizeof(imx219_default_size_regs) / sizeof(struct sensor_regs);
struct sensor_regs imx219_start2[] = { //3280 * 2464 20fps 4lane
    {0x0170, 0x01},
    {0x0171, 0x01},
    {0x0174, 0x00},
    {0x0175, 0x00},
    {0x0176, 0x00},
    {0x0177, 0x00},
    {0x018C, 0x0A},
    {0x018D, 0x0A},
    {0x0301, 0x05},
    {0x0303, 0x01},
    {0x0304, 0x02},
    {0x0305, 0x02},
    {0x0306, 0x00},
    {0x0307, 0x2E},
    {0x0309, 0x0A},
    {0x030B, 0x01},
    {0x030C, 0x00},
    {0x030D, 0x5C},
    {0x455E, 0x00},
    {0x471E, 0x4B},
    {0x4767, 0x0F},
    {0x4750, 0x14},
    {0x4540, 0x00},
    {0x47B4, 0x14},
    {0x4713, 0x30},
    {0x478B, 0x10},
    {0x478F, 0x10},
    {0x4793, 0x10},
    {0x4797, 0x0E},
    {0x479B, 0x0E},
};
const int NUM_CAP_V2REGS_START2 = sizeof(imx219_start2) / sizeof(struct sensor_regs);

struct sensor_regs ov5647[] =
{
    {0x0100, 0x00}, //sleep enable
    {0x0103, 0x01}, //reset
    {0x3034, 0x1a}, //sc cmmn pll ctrl 0
    {0x3035, 0x21}, //sc cmmn pll ctrl 1
    {0x3036, 0x69}, //pll multiplier
    {0x3106, 0xf5}, // srb ctrl 1111 01(pll_sclk/2) 0 (reset arbiter) 1(enable sclk to arbiter)
    {0x3821, 0x00},  //timing_tc_reg21 horizontal bining 0 disable
    {0x3820, 0x00}, //timing_tc_reg20 vertical bining 0 disable
    
    {0x3827, 0xec},
    {0x370c, 0x03},
    {0x3612, 0x5b},
    {0x3618, 0x04},
    
    {0x5000, 0x06}, //ISP ctrl00 1(bc_en: enable)1(wc_en: enable)0
    {0x5002, 0x40}, //ISP ctrl02 10 0(awb_gain_en: disable)
    {0x5003, 0x08}, //ISP ctrl03 1(buf_en: enable)000
    {0x5a00, 0x08}, // DIGC CTRL0 1000 all disabled
    {0x3000, 0x00}, //SC_CMMN_PAD_OEN0 io_y_oen
    {0x3001, 0x00}, //SC_CMMN_PAD_OEN1 io_y_oen
    {0x3002, 0x00}, //SC_CMMN_PAD_OEN2
    {0x3016, 0x08}, // SC_CMMN_MIPI_PHY 1(mipi_pad_enable)000
    {0x3017, 0xe0},//SC_CMMN_MIPI_PHY 11(pgm_vcm High speed common mode voltage)10(driving strength of low speed transmitter)0000
    {0x3018, 0x44},//SC_CMMN_MIPI_SC_CTRL 1(mipi_lane_mode: 1 two lane mode) 0001(mipi_en 1: mipi enable)00
    
    {0x301c, 0xf8},
    {0x301d, 0xf0},
    
    {0x3a18, 0x00}, //AEC GAIN CEILING 248
    {0x3a19, 0xf8}, //AEC GAIN CEILING
    {0x3c01, 0x80}, //50/60 HZ DETECTION CTRL01 1(ban_man_en 1:Manual mode enable)0000000
    {0x3b07, 0x0c}, //STROBE_FREX_MODE_SEL 1(fx1_fm_en)1(frex_inv)00(frex_strobe_mode0)
    {0x380c, 0x0b}, //TIMING_HTS Total horizontal size 2844
    {0x380d, 0x1c}, //TIMING_HTS Total horizontal size
    {0x380e, 0x07}, //TIMING_VTS Total vertical size 1968
    {0x380f, 0xb0}, //TIMING_VTS Total vertical size
    {0x3814, 0x11}, //TIMING_X_INC 001(h_odd_inc)001(h_even_inc)
    {0x3815, 0x11},//TIMING_Y_INC 001(v_odd_inc)001(v_even_inc)
    
    {0x3708, 0x64},
    {0x3709, 0x12},
    
    {0x3808, 0x0a}, //TIMING_X_OUTPUT_SIZE 2592
    {0x3809, 0x20}, //TIMING_X_OUTPUT_SIZE
    {0x380a, 0x07}, //TIMING_Y_OUTPUT_SIZE 1944
    {0x380b, 0x98}, //TIMING_Y_OUTPUT_SIZE
    
    {0x3800, 0x00}, //TIMING_X_ADDR_START
    {0x3801, 0x00}, //TIMING_X_ADDR_START
    {0x3802, 0x00}, //TIMING_Y_ADDR_START
    {0x3803, 0x00}, //TIMING_Y_ADDR_START
    {0x3804, 0x0a}, //TIMING_X_ADDR_END 2623
    {0x3805, 0x3f}, //TIMING_X_ADDR_END
    {0x3806, 0x07}, //TIMING_Y_ADDR_END 1955
    {0x3807, 0xa3}, //TIMING_Y_ADDR_END
    {0x3811, 0x10}, //TIMING_ISP_X_WIN
    {0x3813, 0x06}, //TIMING_ISP_Y_WIN
    
    {0x3630, 0x2e},
    {0x3632, 0xe2},
    {0x3633, 0x23},
    {0x3634, 0x44},
    {0x3636, 0x06},
    {0x3620, 0x64},
    {0x3621, 0xe0},
    {0x3600, 0x37},
    
    {0x3704, 0xa0},
    {0x3703, 0x5a},
    {0x3715, 0x78},
    {0x3717, 0x01},
    {0x3731, 0x02},
    {0x370b, 0x60},
    {0x3705, 0x1a},
    
    {0x3f05, 0x02},
    {0x3f06, 0x10},
    {0x3f01, 0x0a},
    
    {0x3a08, 0x01}, //B50 STEP b50_step 296
    {0x3a09, 0x28}, //B50 STEP b50_step
    {0x3a0a, 0x00},//B60 STEP b50_step 246
    {0x3a0b, 0xf6},//B60 STEP b50_step
    {0x3a0d, 0x08}, //B60 MAX 8
    {0x3a0e, 0x06}, //B50 MAX 6
    {0x3a0f, 0x58}, //WPT Stable range high limit 88
    {0x3a10, 0x50}, //BPT Stable range low limit 80
    {0x3a1b, 0x58}, //WPT2 Table range high limit 88
    {0x3a1e, 0x50}, //BPT2 Stable range low limit 80
    {0x3a11, 0x60}, // High VPT 96
    {0x3a1f, 0x28},//LOW VPT 40
    {0x4001, 0x02}, //BLC CTRL01 start_line
    {0x4004, 0x04}, //BLC CTRL04 blc_line_num
    {0x4000, 0x09}, //BLC CTRL00 1(adc_11bit_mode)0(apply2blackline)0(blackline_averageframe)1(BLC enable)
    {0x4837, 0x16},//PCLK_PERIOD
    {0x4800, 0x24},//MIPI CTRL 00
    {0x3503, 0x03}, //Manual Gain VTS AGC AEC control
    { 0x3820, 0x41 }, // bining timing tc reg20 vertical bining
    { 0x3821, 0x03 }, // bining timing tc reg21 horizontal bining
    
    { 0x350A, 0x00 }, // AGC real gain output high byte 35
    { 0x350B, 0x23 }, // AGC real gain output low byte
    { 0x3212, 0x00 },
    { 0x3500, 0x00 }, //EXPOSURE
    { 0x3501, 0x04 }, //EXPOSURE
    { 0x3502, 0x60 }, //EXPOSURE
    { 0x3212, 0x10 },
    { 0x3212, 0xA0 }
    
    //   { 0x0100, 0x01 } //sleep
};
struct sensor_regs ov5647_start[] = {
    { 0x0100, 0x01 }
};
struct sensor_regs ov5647_stop[] = {
    { 0x0100, 0x00 }
};

const int NUM_REGS_START = sizeof(ov5647) / sizeof(struct sensor_regs);

const int NUM_REGS_STOP = sizeof(ov5647_stop) / sizeof(struct sensor_regs);
MMAL_COMPONENT_T *rawcam;
char * directory;
char * previewOutput;
int total_frame_count = 100;
unsigned int gain_val = 50;
int devide_buffer_by = 2;
int write_step = 2;

#define PI_REVISION_2_1 "a21041"
#define PI_REVISION_2_2 "a01041"
#define PI_REVISION_2_3 "2a21041"
#define PI_REVISION_2_4 "2a01041"

#define PI_REVISION_3_1 "a02082"
#define PI_REVISION_3_2 "a22082"
#define PI_REVISION_3_3 "2a02082"
#define PI_REVISION_3_4 "2a22082"

void checkVersion();
char * i2c_file;
#define CommandOutput       4
#define CommandFrameStep    5
#define CommandWriteThread    6
#define CommandSkip    7
#define CommandFrameCount    8
#define CommandGain    9
#define CommandWidth 10
#define CommandHeight 11
#define CommandXStart 12
#define CommandYStart 13
#define CommandHTS 14
#define CommandVTS 15
#define CommandBufferDivide 16
#define CommandWriteStep 17
#define CommandExposure 18
#define CommandPreview 19
#define CommandPreviewOutput 20
#define CommandCompressionRatio 21
#define CommandFormat 22
#define CommandEncodingBit 23
static COMMAND_LIST cmdline_commands[] =
{
    { CommandOutput,        "-output",     "o",  "Output directory <directory name> (to write to stdout, use '-o -')", 1 },
    { CommandFrameStep,        "-frstep",     "fs",  "Frame Step: every n frame in 25fps", 1 },
    { CommandWriteThread,        "-wrthread",     "wt",  "Use writethread", 0},
    { CommandSkip,        "-skip",     "sk",  "Initali frames to skip", 1},
    { CommandFrameCount,        "-framecount",     "fc",  "Number of frames to record", 1},
    { CommandGain,        "-gain",     "g",  "set Gain 0 to 255", 1},
    { CommandWidth,         "-width",      "w",  "Set image width <size>. Default 1920", 1 },
    { CommandHeight,        "-height",     "h",  "Set image height <size>. Default 1080", 1 },
    { CommandXStart,        "-xstart",     "xs",  "x start address. Default 0", 1 },
    { CommandYStart,        "-ystart",     "ys",  "y start address. Default 0", 1 },
    { CommandHTS,        "-hts",     "hts",  "hts total horizontal size 3816", 1 },
    { CommandVTS,        "-vts",     "vts",  "vts total vertical size 5632", 1 },
    { CommandBufferDivide,        "-dv",     "dv",  "Devide buffer by input Default 0", 1 },
    { CommandWriteStep,        "-ws",     "ws",  "Wirte buffer node at every frame Default 0", 1 },
    { CommandExposure,        "-exposure",     "e",  "Exposure max 1000000", 1 },
    { CommandPreview,        "-preview",     "p",  "Preview Mode", 0 },
    { CommandPreviewOutput,        "-previewoutput",     "po",  "Preview Output", 1 },
    { CommandCompressionRatio,      "-compressionratio",    "cr",   "Compression Ration 0 to 9", 1},
    { CommandFormat,        "-format", "f", "Format 0:RAW 1:H264", 1},
    { CommandEncodingBit,   "-encodingbit", "eb", "Encoding raw in 8 or 10bit", 1},
};
static int cmdline_commands_size = sizeof(cmdline_commands) / sizeof(cmdline_commands[0]);

static int parse_cmdline(int argc, const char **argv)
{
    int i;
    int valid = 1;
    for (i=1 ; i < argc ; i++)
    {
        int num_parameters;
        if (!argv[i])
            continue;
        
        if (argv[i][0] != '-')
        {
            valid = 0;
            continue;
        }
        int command_id = raspicli_get_command_id(cmdline_commands, cmdline_commands_size, &argv[i][1], &num_parameters);
        
        // If we found a command but are missing a parameter, continue (and we will drop out of the loop)
        if (command_id != -1 && num_parameters > 0 && (i + 1 >= argc) )
            continue;
        
        //  We are now dealing with a command line option
        switch (command_id)
        {
                
            case CommandOutput:  // output filename
            {
                int len = strlen(argv[i + 1]);
                if (len)
                {
                    directory = malloc(len + 1);
                    vcos_assert(directory);
                    if (directory)
                        strncpy(directory, argv[i + 1], len+1);
                    i++;
                    //ofile = fopen(directory, "wb");
                    dfile = open(directory,  O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
                    //dumfile = fopen("/home/pi/dum.data", "wb");
                    fprintf(stdout, "Output directory given %s\n", directory);
                    fflush(stdout);
                }
                else
                    valid = 0;
                break;
            }
            case CommandFrameStep:
            {
                if (sscanf(argv[i + 1], "%u", &frame_step) != 1)
                    valid = 0;
                else
                    i++;
                fprintf(stdout, "FRS is %d\n", frame_step);
                fflush(stdout);
                break;
            }
            case CommandWriteThread:
            {
                use_write_thread = 1;
                fprintf(stdout, "use_write_thread is %d\n", use_write_thread);
                fflush(stdout);
                break;
            }
            case CommandSkip:
            {
                
                if (sscanf(argv[i + 1], "%u", &initial_frames_to_skip) != 1)
                    valid = 0;
                else
                    i++;
                fprintf(stdout, "skip %d frames\n", initial_frames_to_skip);
                fflush(stdout);
                break;
            }
            case CommandGain:
            {
                if (sscanf(argv[i + 1], "%u", &gain_val) != 1)
                    valid = 0;
                else
                    i++;
                fprintf(stdout, "Gain:%d\n", gain_val);
                fflush(stdout);
                break;
            }
            case CommandFrameCount:
            {
                if (sscanf(argv[i + 1], "%u", &total_frame_count) != 1)
                    valid = 0;
                else
                    i++;
                fprintf(stdout, "%d frames to be recorded\n", total_frame_count);
                fflush(stdout);
                break;
            }
            case CommandWidth:
            {
                if (sscanf(argv[i + 1], "%u", &width) != 1)
                    valid = 0;
                else
                    i++;
                fprintf(stdout, "Width %d\n", width);
                fflush(stdout);
                break;
            }
            case CommandHeight:
            {
                if (sscanf(argv[i + 1], "%u", &height) != 1)
                    valid = 0;
                else
                    i++;
                fprintf(stdout, "Height %d\n", height);
                fflush(stdout);
                break;
            }
            case CommandXStart:
            {
                if (sscanf(argv[i + 1], "%u", &x_start) != 1)
                    valid = 0;
                else
                    i++;
                fprintf(stdout, "X start address %d\n", x_start);
                fflush(stdout);
                break;
            }
            case CommandYStart:
            {
                if (sscanf(argv[i + 1], "%u", &y_start) != 1)
                    valid = 0;
                else
                    i++;
                fprintf(stdout, "Y start address %d\n", y_start);
                fflush(stdout);
                break;
            }
            case CommandHTS:
            {
                if (sscanf(argv[i + 1], "%u", &hts) != 1)
                    valid = 0;
                else
                    i++;
                fprintf(stdout, "HTS %d\n", hts);
                fflush(stdout);
                break;
            }
            case CommandVTS:
            {
                if (sscanf(argv[i + 1], "%u", &vts) != 1)
                    valid = 0;
                else
                    i++;
                fprintf(stdout, "VTS %d\n", vts);
                fflush(stdout);
                break;
            }
            case CommandBufferDivide:
            {
                if (sscanf(argv[i + 1], "%u", &devide_buffer_by) != 1)
                    valid = 0;
                else
                    i++;
                pre_alloc = 0;
                fprintf(stdout, "Buffer node devided by %d\n", devide_buffer_by);
                fflush(stdout);
                break;
            }
            case CommandWriteStep:
            {
                if (sscanf(argv[i + 1], "%u", &write_step) != 1)
                    valid = 0;
                else
                    i++;
                pre_alloc = 0;
                fprintf(stdout, "Write step %d\n", write_step);
                fflush(stdout);
                break;
            }
            case CommandExposure:
            {
                if (sscanf(argv[i + 1], "%u", &exposure) != 1)
                    valid = 0;
                else
                    i++;
                if(exposure > 0xFFFFF) {
                    exposure = 0xFFFFF;
                }
                fprintf(stdout, "Exposure %d\n", exposure);
                fflush(stdout);
                break;
            }
            case CommandPreview:
            {
                previewMode = 1;
                fprintf(stdout, "Preview enabled %d\n", previewMode);
                fflush(stdout);
                break;
            }
            case CommandPreviewOutput:  // output filename
            {
                int len = strlen(argv[i + 1]);
                if (len)
                {
                    previewOutput = malloc(len + 1);
                    vcos_assert(previewOutput);
                    if (previewOutput)
                        strncpy(previewOutput, argv[i + 1], len+1);
                    i++;
                    fprintf(stdout, "Preview Output given %s\n", previewOutput);
                    fflush(stdout);
                }
                else
                    valid = 0;
                break;
            }
            case CommandCompressionRatio:
            {
                if (sscanf(argv[i + 1], "%u", &compRatio) != 1)
                    valid = 0;
                else
                    i++;
                pre_alloc = 0;
                fprintf(stdout, "Compression Ratio %d\n", compRatio);
                fflush(stdout);
                break;
            }
            case CommandFormat:
            {
                if (sscanf(argv[i + 1], "%u", &format) != 1)
                    valid = 0;
                else
                    i++;
                fprintf(stdout, "Format is set to %d\n", format);
                fflush(stdout);
                break;
            }
            case CommandEncodingBit:
            {
                if(sscanf(argv[i+1], "%u", &encodingBit) != 1) {
                    valid = 0;
                }
                else
                    i++;
                if(encodingBit != 8 && encodingBit != 10)
                {
                    printf("Encoding bit %d is not valid so sets to 10 bit\n", encodingBit);
                    encodingBit = 10;
                }
                printf("EncodingBit Value set to %d\n", encodingBit);
                break;
            }
        }
    }
    return 0;
}
int i2c_write(int handle, unsigned char * msg, int nbyte)
{
    if(write(handle, msg, nbyte) != nbyte)
    {
        vcos_log_error("Failed to write try again %s\n", strerror(errno));
    } else {
        vcos_log_error("I2C write : msg %x %x %x\n", msg[0], msg[1], msg[2]);
    }
    //   vcos_log_error("i2c_write success to write\n");
    return 0;
}
int stanby(unsigned char rdval)
{
    int fd, i;
    fd = open(i2c_file, O_RDWR);
    if (!fd)
    {
        vcos_log_error("Couldn't open I2C device");
        return 0;
    }
    if(ioctl(fd, I2C_SLAVE, SLAVE_ADDR) < 0)
    {
        vcos_log_error("stby Failed to set I2C address");
        return 0;
    }
    unsigned char msg[3];
    *((unsigned short*)&msg) = ((0xFF00&(ov5647_start[0].reg<<8)) + (0x00FF&(ov5647_start[0].reg>>8)));
    msg[2] = rdval&0xFE;
    if(i2c_write(fd, msg, 3) != 0)
    {
        vcos_log_error("Failed to read stby");
        return 0;
    }
    return 1;
}
/*
 int read_sensor(void)
 {
 int fd, i;
 fd = open(i2c_file, O_RDWR);
 if (!fd)
 {
 vcos_log_error("Couldn't open I2C device");
 return 0;
 }
 if(ioctl(fd, I2C_SLAVE, SLAVE_ADDR) < 0)
 {
 vcos_log_error("read_sensor Failed to set I2C address");
 return 0;
 }
 unsigned char rdval;
 unsigned char msg[3];
 *((unsigned short*)&msg) = ((0xFF00&(ov5647_start[0].reg<<8)) + (0x00FF&(ov5647_start[0].reg>>8)));
 msg[2] = 0x00;
 
 int ret = read(fd, msg, 3);
 if(ret != 3)
 {
 vcos_log_error("Failed to read stby ret %d", ret);
 return 0;
 }
 vcos_log_error("stby read value 0x0100 is %x", msg[2]);
 return stanby(msg[2]);
 }
 */
void wake_up(void)
{
    int fd, i;
    fd = open(i2c_file, O_RDWR);
    if (!fd)
    {
        vcos_log_error("Couldn't open I2C device");
        return;
    }
    if(ioctl(fd, I2C_SLAVE, SLAVE_ADDR) < 0)
    {
        vcos_log_error("Failed to set I2C address");
        return;
    }
    for (i=0; i<NUM_REGS_STOP; i++)
    {
        unsigned char msg[3];
        *((unsigned short*)&msg) = ((0xFF00&(ov5647_start[i].reg<<8)) + (0x00FF&(ov5647_start[i].reg>>8)));
        msg[2] = ov5647_start[i].data;
        if(i2c_write(fd, msg, 3) != 0)
        {
            vcos_log_error("Failed to write register index %d", i);
        }
        
    }
    close(fd);
#ifdef DO_PIN_CONFIG
    digitalWrite(41, 0); //Shutdown pin on B+ and Pi2
    digitalWrite(32, 0); //LED pin on B+ and Pi2
#endif
    uint64_t timeStamp;
    if(mmal_port_parameter_get_uint64(rawcam->output[0], MMAL_PARAMETER_SYSTEM_TIME, &timeStamp) == MMAL_SUCCESS)
    {
        uint64_t cur_time = timeStamp/1000;
        //fprintf(stdout, "started at %lld\n", cur_time);
        //fflush(stdout);
        prev_time = cur_time;
        
    }
}

void start_camera_streaming(void)
{
    /*
     if(read_sensor() == 0)
     {
     vcos_log_error("STBY failed");
     }
     */
    int fd, i;
    
#ifdef DO_PIN_CONFIG
    wiringPiSetupGpio();
    pinModeAlt(0, INPUT);
    pinModeAlt(1, INPUT);
    //Toggle these pin modes to ensure they get changed.
    pinModeAlt(28, INPUT);
    pinModeAlt(28, 4);	//Alt0
    pinModeAlt(29, INPUT);
    pinModeAlt(29, 4);	//Alt0
    digitalWrite(41, 1); //Shutdown pin on B+ and Pi2
    digitalWrite(32, 1); //LED pin on B+ and Pi2
#endif
    fd = open(i2c_file, O_RDWR);
    if (!fd)
    {
        vcos_log_error("Couldn't open I2C device");
        return;
    }
    //   if(ioctl(fd, I2C_SLAVE, 0x36) < 0)
    if(ioctl(fd, I2C_SLAVE, SLAVE_ADDR) < 0)
    {
        vcos_log_error("Failed to set I2C address");
        return;
    }
    
    for (i=0; i<NUM_CAP_V2REGS_START1; i++)
    {
        unsigned char msg[3];
        
        *((unsigned short*)&msg) = ((0xFF00&(imx219_start1[i].reg<<8)) + (0x00FF&(imx219_start1[i].reg>>8)));
        msg[2] = imx219_start1[i].data;
        if(i2c_write(fd, msg, 3) != 0)
        {
            vcos_log_error("Failed to write register index %d", i);
        }
        //      fprintf(stdout, "writing data at %x,%x, %x\n", msg[0], msg[1], msg[2]);
        //      fflush(stdout);
        char cont;
        //  cont = getchar();
    }
    if(gain_val > 0)
    {
        unsigned char msg[3];
        
        *((unsigned short*)&msg) = ((0xFF00&(0x0157<<8)) + (0x00FF&(0x0157>>8)));
        msg[2] = gain_val;
        if(i2c_write(fd, msg, 3) != 0)
        {
            vcos_log_error("Failed to write register for GAIN\n");
        }
        //      fprintf(stdout, "writing GAIN data at %x,%x, %x\n", msg[0], msg[1], msg[2]);
        //      fflush(stdout);
    }
    /*
     for (i=0; i<NUM_CAP_V2REGS_DEF_FPS; i++)
     {
     unsigned char msg[3];
     
     *((unsigned short*)&msg) = ((0xFF00&(imx219_default_fps_regs[i].reg<<8)) + (0x00FF&(imx219_default_fps_regs[i].reg>>8)));
     msg[2] = imx219_default_fps_regs[i].data;
     if(i2c_write(fd, msg, 3) != 0)
     {
     vcos_log_error("Failed to write register index %d", i);
     fflush(stdout);
     }
     //      fprintf(stdout, "writing data at %x,%x, %x\n", msg[0], msg[1], msg[2]);
     //      fflush(stdout);
     char cont;
     //  cont = getchar();
     }
     */
    for (i=0; i<NUM_CAP_V2REGS_START2; i++)
    {
        unsigned char msg[3];
        
        *((unsigned short*)&msg) = ((0xFF00&(imx219_start2[i].reg<<8)) + (0x00FF&(imx219_start2[i].reg>>8)));
        msg[2] = imx219_start2[i].data;
        if(i2c_write(fd, msg, 3) != 0)
        {
            vcos_log_error("Failed to write register index %d", i);
            fflush(stdout);
        }
        //      fprintf(stdout, "writing data at %x,%x, %x\n", msg[0], msg[1], msg[2]);
        //      fflush(stdout);
        char cont;
        //  cont = getchar();
    }
    
    
    close(fd);
}

void set_size_v2(void)
{
    int fd, i;
    fd = open(i2c_file, O_RDWR);
    if (!fd)
    {
        vcos_log_error("Couldn't open I2C device");
        return;
    }
    if(ioctl(fd, I2C_SLAVE, SLAVE_ADDR) < 0)
    {
        vcos_log_error("Failed to set I2C address");
        return;
    }
    
    //int frame_length = hts;
    if(exposure > 0)
    {
        exposure >>= 4;
        unsigned char expMsg1[3];
        *((unsigned short*)&expMsg1) = ((0xFF00&(0x015a<<8)) + (0x00FF&(0x015a>>8)));
        expMsg1[2] = (0xFF00&(exposure))>>8;
        if(i2c_write(fd, expMsg1, 3) != 0)
        {
            vcos_log_error("Failed to write register for exposure higher\n");
        }
        unsigned char expMsg2[3];
        *((unsigned short*)&expMsg2) = ((0xFF00&(0x015b<<8)) + (0x00FF&(0x015b>>8)));
        expMsg2[2] = 0x00FF&(exposure);
        if(i2c_write(fd, expMsg2, 3) != 0)
        {
            vcos_log_error("Failed to write register for exposure lower\n");
        }
        /*
        int shutter = exposure;
        if(shutter > vts-4)
            hts = shutter+4;
        else
            hts = vts;
         */
        fprintf(stdout, "Exposure set to %d\n", exposure);
        
    }
    fprintf(stdout, "HTS (frame_length) to %d\n", hts);
    //HTS
    unsigned char htsMsg1[3];
    *((unsigned short*)&htsMsg1) = ((0xFF00&(0x0160<<8)) + (0x00FF&(0x0160>>8)));
    htsMsg1[2] = (0xFF00&(hts))>>8;
    if(i2c_write(fd, htsMsg1, 3) != 0)
    {
        vcos_log_error("Failed to write register for x start higher\n");
    }
    unsigned char htsMsg2[3];
    *((unsigned short*)&htsMsg2) = ((0xFF00&(0x0161<<8)) + (0x00FF&(0x0161>>8)));
    htsMsg2[2] = 0x00FF&(hts);
    if(i2c_write(fd, htsMsg2, 3) != 0)
    {
        vcos_log_error("Failed to write register for x start lower\n");
    }
    
    //VTS
    unsigned char vtsMsg1[3];
    *((unsigned short*)&vtsMsg1) = ((0xFF00&(0x0162<<8)) + (0x00FF&(0x0162>>8)));
    vtsMsg1[2] = (0xFF00&(vts))>>8;
    if(i2c_write(fd, vtsMsg1, 3) != 0)
    {
        vcos_log_error("Failed to write register for x start higher\n");
    }
    unsigned char vtsMsg2[3];
    *((unsigned short*)&vtsMsg2) = ((0xFF00&(0x0163<<8)) + (0x00FF&(0x0163>>8)));
    vtsMsg2[2] = 0x00FF&(vts);
    if(i2c_write(fd, vtsMsg2, 3) != 0)
    {
        vcos_log_error("Failed to write register for x start lower\n");
    }
    
    //Set Size
    //x start
    unsigned char xStartMsg1[3];
    *((unsigned short*)&xStartMsg1) = ((0xFF00&(0x0164<<8)) + (0x00FF&(0x0164>>8)));
    xStartMsg1[2] = (0xFF00&(x_start))>>8;
    if(i2c_write(fd, xStartMsg1, 3) != 0)
    {
        vcos_log_error("Failed to write register for x start higher\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", xStartMsg1[0], xStartMsg1[1], xStartMsg1[2]);
    
    unsigned char xStartMsg2[3];
    *((unsigned short*)&xStartMsg2) = ((0xFF00&(0x0165<<8)) + (0x00FF&(0x0165>>8)));
    xStartMsg2[2] = 0x00FF&(x_start);
    if(i2c_write(fd, xStartMsg2, 3) != 0)
    {
        vcos_log_error("Failed to write register for x start lower\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", xStartMsg2[0], xStartMsg2[1], xStartMsg2[2]);
    
    x_end = x_start + width -1;
    //x end
    unsigned char xEndMsg1[3];
    *((unsigned short*)&xEndMsg1) = ((0xFF00&(0x0166<<8)) + (0x00FF&(0x0166>>8)));
    xEndMsg1[2] = (0xFF00&(x_end))>>8;
    if(i2c_write(fd, xEndMsg1, 3) != 0)
    {
        vcos_log_error("Failed to write register for x end higher\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", xEndMsg1[0], xEndMsg1[1], xEndMsg1[2]);
    
    
    unsigned char xEndMsg2[3];
    *((unsigned short*)&xEndMsg2) = ((0xFF00&(0x0167<<8)) + (0x00FF&(0x0167>>8)));
    xEndMsg2[2] =0x00FF&(x_end);
    if(i2c_write(fd, xEndMsg2, 3) != 0)
    {
        vcos_log_error("Failed to write register for x end lower\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", xEndMsg2[0], xEndMsg2[1], xEndMsg2[2]);
    
    
    //y start
    unsigned char yStartMsg1[3];
    *((unsigned short*)&yStartMsg1) = ((0xFF00&(0x0168<<8)) + (0x00FF&(0x0168>>8)));
    yStartMsg1[2] = (0xFF00&(y_start))>>8;
    if(i2c_write(fd, yStartMsg1, 3) != 0)
    {
        vcos_log_error("Failed to write register for y start higher\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", yStartMsg1[0], yStartMsg1[1], yStartMsg1[2]);
    
    
    unsigned char yStartMsg2[3];
    *((unsigned short*)&yStartMsg2) = ((0xFF00&(0x0169<<8)) + (0x00FF&(0x0169>>8)));
    yStartMsg2[2] = 0x00FF&(y_start);
    if(i2c_write(fd, yStartMsg2, 3) != 0)
    {
        vcos_log_error("Failed to write register for y start lower\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", yStartMsg2[0], yStartMsg2[1], yStartMsg2[2]);
    
    y_end = y_start + height -1;
    //y end
    unsigned char yEndMsg1[3];
    *((unsigned short*)&yEndMsg1) = ((0xFF00&(0x016A<<8)) + (0x00FF&(0x016A>>8)));
    yEndMsg1[2] =(0xFF00&(y_end-1))>>8;
    if(i2c_write(fd, yEndMsg1, 3) != 0)
    {
        vcos_log_error("Failed to write register for y end higher\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", yEndMsg1[0], yEndMsg1[1], yEndMsg1[2]);
    
    unsigned char yEndMsg2[3];
    *((unsigned short*)&yEndMsg2) = ((0xFF00&(0x016B<<8)) + (0x00FF&(0x016B>>8)));
    yEndMsg2[2] =0x00FF&(y_end-1);
    if(i2c_write(fd, yEndMsg2, 3) != 0)
    {
        vcos_log_error("Failed to write register for y end lower\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", yEndMsg2[0], yEndMsg2[1], yEndMsg2[2]);
    
    //width
    unsigned char widthMsg1[3];
    *((unsigned short*)&widthMsg1) = ((0xFF00&(0x016C<<8)) + (0x00FF&(0x016C>>8)));
    widthMsg1[2] = (0xFF00&width)>>8;
    if(i2c_write(fd, widthMsg1, 3) != 0)
    {
        vcos_log_error("Failed to write register for width higher\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", widthMsg1[0], widthMsg1[1], widthMsg1[2]);
    
    unsigned char widthMsg2[3];
    *((unsigned short*)&widthMsg2) = ((0xFF00&(0x016D<<8)) + (0x00FF&(0x016D>>8)));
    widthMsg2[2] = 0x00FF&width;
    if(i2c_write(fd, widthMsg2, 3) != 0)
    {
        vcos_log_error("Failed to write register for width lower\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", widthMsg2[0], widthMsg2[1], widthMsg2[2]);
    
    //height
    unsigned char heightMsg1[3];
    *((unsigned short*)&heightMsg1) = ((0xFF00&(0x016E<<8)) + (0x00FF&(0x016E>>8)));
    heightMsg1[2] = (0xFF00&height)>>8;
    if(i2c_write(fd, heightMsg1, 3) != 0)
    {
        vcos_log_error("Failed to write register for height higher\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", heightMsg1[0], heightMsg1[1], heightMsg1[2]);
    
    
    unsigned char heightMsg2[3];
    *((unsigned short*)&heightMsg2) = ((0xFF00&(0x016F<<8)) + (0x00FF&(0x016F>>8)));
    heightMsg2[2] = 0x00FF&height;
    if(i2c_write(fd, heightMsg2, 3) != 0)
    {
        vcos_log_error("Failed to write register for height lower\n");
    }
    //   fprintf(stdout, "writing data at %x,%x, %x\n", heightMsg2[0], heightMsg2[1], heightMsg2[2]);
    //   fflush(stdout);
    close(fd);
    fprintf(stdout, "RawSize:%d:%d:%d:%d:%d:%d:%d\n", x_start, x_end, y_start, y_end, hts, vts,total_frame_count);
    fflush(stdout);
}
void stop_camera_streaming(void)
{
    int fd, i;
    fd = open(i2c_file, O_RDWR);
    if (!fd)
    {
        vcos_log_error("Couldn't open I2C device");
        return;
    }
    //if(ioctl(fd, I2C_SLAVE, 0x36) < 0)
    if(ioctl(fd, I2C_SLAVE, SLAVE_ADDR) < 0)
    {
        vcos_log_error("Failed to set I2C address");
        return;
    }
    for (i=0; i<NUM_REGS_STOP; i++)
    {
        unsigned char msg[3];
        *((unsigned short*)&msg) = ((0xFF00&(ov5647_stop[i].reg<<8)) + (0x00FF&(ov5647_stop[i].reg>>8)));
        msg[2] = ov5647_stop[i].data;
        if(i2c_write(fd, msg, 3) != 0)
        {
            vcos_log_error("Failed to write register index %d", i);
        }
    }
    close(fd);
#ifdef DO_PIN_CONFIG
    digitalWrite(41, 0); //Shutdown pin on B+ and Pi2
    digitalWrite(32, 0); //LED pin on B+ and Pi2
#endif
}
int running = 0;
static void encoder_buffer_callback(MMAL_PORT_T * port,  MMAL_BUFFER_HEADER_T *buffer)
{
    MMAL_STATUS_T status;
    uint64_t timeStamp;
    mmal_port_parameter_get_uint64(port, MMAL_PARAMETER_SYSTEM_TIME, &timeStamp);
    vcos_log_error("encoder_buffer_callback Buffer %p returned, %llu, filled %d, timestamp %llu, flags %04X, running %d", buffer, timeStamp/1000, buffer->length, buffer->pts, buffer->flags, running);
    int bytes_written = buffer->length;

    vcos_log_error("running %d", running);
    if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG)
    {
        vcos_log_error("config data");
    }
    else if ((buffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO))
    {
        vcos_log_error("config codec info");
    }
    else if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)
    {
        vcos_log_error("config key frame");
    }
    else if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END)
    {
        vcos_log_error("config frame end");
    }

    if(running)
    {
        //mmal_buffer_header_mem_lock(buffer);
        //FILE * file = (FILE*)port->userdata;
        //bytes_written = fwrite(buffer->data, 1, buffer->length, dfile);
        //fflush(dfile);
        bytes_written = write(dfile, buffer->data, buffer->length);
        //fwrite(buffer->data, buffer->length, 1, dfile);
        mmal_buffer_header_mem_unlock(buffer);
        if(bytes_written != buffer->length)
        {
            vcos_log_error("Failed to write buffer data (%d from %d) - aborting", bytes_written, buffer->length);
        }
        mmal_buffer_header_release(buffer);
        status = mmal_port_send_buffer(port, buffer);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("mmal_port_send_buffer failed on buffer %p, status %d", buffer, status);
        }
    }
    //mmal_buffer_header_release(buffer);
    //mmal_port_send_buffer(port, buffer);
       // and send one back to the port (if still open)
/*
   if (port->is_enabled)
   {
      MMAL_STATUS_T status;
      MMAL_BUFFER_HEADER_T *new_buffer;
      new_buffer = mmal_queue_get(pool->queue);

      if (new_buffer)
         status = mmal_port_send_buffer(port, new_buffer);

      if (!new_buffer || status != MMAL_SUCCESS)
         vcos_log_error("Unable to return a buffer to the encoder port");
   }
*/
}

static void preview_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    static int count = 0;
    static int frame_count = 1;
    //vcos_log_error("Buffer %p returned, filled %d, timestamp %llu, flags %04X", buffer, buffer->length, buffer->pts, buffer->flags);
    if(running)
    {
        
        uint64_t timeStamp;
        if(!(buffer->flags&MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO)  && mmal_port_parameter_get_uint64(rawcam->output[0], MMAL_PARAMETER_SYSTEM_TIME, &timeStamp) == MMAL_SUCCESS)
        {
            uint64_t cur_time = timeStamp/1000;
            uint64_t first_buffer_took = (timeStamp - record_start_pressed_time)/1000;
            if(count == 0)
                fprintf(stdout, "FirstBufferTook:%lld\n", first_buffer_took);
            /*
             else
             fprintf(stdout, "add buffer:%ld\n", firstBufferTimeConsumption);
             */
            if(cur_time, cur_time-prev_time >= 100) {
                fprintf(stdout, "############## Took long #########################################\n");
                
            }
            fprintf(stdout, "%d %lld diff = %lld frs = %d\n", count, cur_time, cur_time-prev_time, frame_step );
            fflush(stdout);
            prev_time = cur_time;
            
        }
        
        int skip = 0;

        if(!(buffer->flags&MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) )
        {
            // Save every 15th frame
            // SD card access is to slow to do much more.
            //uint64_t timeStamp = vcos_getmicrosecs64()/1000;
            //if(mmal_port_parameter_get_uint64(rawcam->output[0], MMAL_PARAMETER_SYSTEM_TIME, &timeStamp) == MMAL_SUCCESS)
            {
                uint64_t cur_time = vcos_getmicrosecs64()/1000;
                if(frame_count == 1)
                    fprintf(stdout, "FirstBufferArrival:%lld\n", cur_time);
                //else
                //	fprintf(stdout, "add buffer:%lld\n", cur_time);
                
                fflush(stdout);
            }
            fprintf(stdout, "Buffer arrived\n");
            struct stat st;
            int result = stat(previewOutput, &st);
            fprintf(stdout, "read result %d\n", result);
            if(result == 0)
            {
                fprintf(stdout, "preview out exists so skip\n");
                //File exist
                //Skip
            }
            else
            {
                FILE *file;
                file = fopen(previewOutput, "wb");
                if(file)
                {
                    fwrite(buffer->data, buffer->length, 1, file);
                    fclose(file);
                    fprintf(stdout, "PreviewReady\n");
                }
            }
        }
        if(skip == 0)
        {
            buffer->length = 0;
            mmal_port_send_buffer(port, buffer);
        }
        
    }
    else
        mmal_buffer_header_release(buffer);
}

static void callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    static int count = 0;
    static int frame_count = 1;
    //vcos_log_error("Buffer %p returned, filled %d, timestamp %llu, flags %04X", buffer, buffer->length, buffer->pts, buffer->flags);
    if(running)
    {
        
        uint64_t timeStamp;
        if(!(buffer->flags&MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO)  && mmal_port_parameter_get_uint64(rawcam->output[0], MMAL_PARAMETER_SYSTEM_TIME, &timeStamp) == MMAL_SUCCESS)
        {
            uint64_t cur_time = timeStamp/1000;
            uint64_t first_buffer_took = (timeStamp - record_start_pressed_time)/1000;
            if(count == 0)
                fprintf(stdout, "FirstBufferTook:%lld\n", first_buffer_took);
            /*
             else
             fprintf(stdout, "add buffer:%ld\n", firstBufferTimeConsumption);
             */
            if(cur_time, cur_time-prev_time >= 110) {
                fprintf(stdout, "############## Took long #########################################\n");
                
            }
            fprintf(stdout, "FrameTook:%lld\n", cur_time-prev_time);
//            fprintf(stdout, "%d %lld diff = %lld frs = %d\n", count, cur_time, cur_time-prev_time, frame_step );
            fflush(stdout);
            prev_time = cur_time;
            
        }
        
        int skip = 0;
        
         if(!(buffer->flags&MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) &&
           (((count++)%frame_step)==0) && count > initial_frames_to_skip && buffer_queue_index < total_frame_count)
        {
            // Save every 15th frame
            // SD card access is to slow to do much more.
            //uint64_t timeStamp = vcos_getmicrosecs64()/1000;
            //if(mmal_port_parameter_get_uint64(rawcam->output[0], MMAL_PARAMETER_SYSTEM_TIME, &timeStamp) == MMAL_SUCCESS)
            {
                uint64_t cur_time = vcos_getmicrosecs64()/1000;
                if(frame_count == 1)
                    fprintf(stdout, "FirstBufferArrival:%lld\n", cur_time);
                //else
                //	fprintf(stdout, "add buffer:%lld\n", cur_time);
                
                fflush(stdout);
            }
/*
            if(compRatio > 0)
            {
                uint64_t time_before_compression = vcos_getmicrosecs64()/1000;
                Bytef * compressBuf = (Bytef * )malloc(buffer->length);
                uLongf complen = buffer->length;
                fprintf(stdout, "Compress start with ratio %d\n", compRatio);
                int compVal = compress2(compressBuf, &complen, buffer->data, buffer->length, compRatio);
                uint64_t time_after_compression = vcos_getmicrosecs64()/1000 - time_before_compression;
                fprintf(stdout, "Compressed result %d length %d and took %lld\n", compVal, complen, time_after_compression);
            }
*/
            if(pre_alloc && devide_buffer_by > 0)
            {
                add_buffer_to_node_chain_devided(0, buffer, port);
            }
            else if(pre_alloc == 0 && devide_buffer_by > 0)
            {
                add_buffer_node_divided(0, buffer, port);
            }
            else if(pre_alloc)
            {
                add_buffer_to_node_chain(0, buffer, port);
            }
            else if(use_write_thread)
            {
                add_buffer_node(0, buffer, port);
                skip = 1;
            }
            else
            {
                FILE *file;
                char filename[50];
                sprintf(filename, "%s%d.raw", directory, frame_count);
                file = fopen(filename, "wb");
                if(file)
                {
                    fwrite(buffer->data, buffer->length, 1, file);
                    fclose(file);
                }
            }
            frame_count++;
        }
        if(skip == 0)
        {
            buffer->length = 0;
            mmal_port_send_buffer(port, buffer);
            
            if(write_step > 0 && !(buffer->flags&MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) && pre_alloc == 0 && devide_buffer_by > 0 && buffer_queue_index%write_step == 1)
            {
                if(use_write_thread)
                {
                    fprintf(stdout, "Give signal to write thread to write one buffer\n");
                    pthread_cond_signal(&condAvail);
                }
                else
                {
                    fprintf(stdout, "Index %d\n", buffer_queue_index);
                    write_first_node();
                }
            }
            
        }
        
    }
    else
        mmal_buffer_header_release(buffer);
}


int main (int argc, const char **argv)
{
    //	MMAL_COMPONENT_T *rawcam;
    MMAL_STATUS_T status;
    MMAL_PORT_T *output, *input, *isp_input, *isp_output, *encoder_input, *encoder_output;
//    MMAL_POOL_T *pool;
    MMAL_COMPONENT_T * render, *isp, *splitter, *encoder;
    MMAL_CONNECTION_T *connection[4] = {0};
    MMAL_PARAMETER_CAMERA_RX_CONFIG_T rx_cfg = {{MMAL_PARAMETER_CAMERA_RX_CONFIG, sizeof(rx_cfg)}};
    MMAL_PARAMETER_CAMERA_RX_TIMING_T rx_timing = {{MMAL_PARAMETER_CAMERA_RX_TIMING, sizeof(rx_timing)}};
    int i;
    
    checkVersion();
    bcm_host_init();
    vcos_log_register("RaspiRaw", VCOS_LOG_CATEGORY);
    pthread_t write_thread;
    // Parse the command line and put options in to our status structure
    if (parse_cmdline(argc, argv))
    {
        //status = -1;
        //exit(EX_USAGE);
    }
    x_start = (WIDTH-width)/2;
    y_start = (HEIGHT-height)/2;
    
    vcos_log_error("Default w=%d h=%d x_start=%d y_start=%d\n", width, height, x_start, y_start);
    vcos_log_error("Create component vc.ril.rawcam");
    status = mmal_component_create("vc.ril.rawcam", &rawcam);
    //status = mmal_component_create("vc.ril.camera", &rawcam);

    uint64_t timeStamp;
    if(mmal_port_parameter_get_uint64(rawcam->output[0], MMAL_PARAMETER_SYSTEM_TIME, &timeStamp) == MMAL_SUCCESS)
    {
        fprintf(stdout, "start %lld \n", timeStamp/1000);
        fflush(stdout);
        
    }
    if(status != MMAL_SUCCESS)
    {
        vcos_log_error("Failed to create rawcam");
        return -1;
    }
    if(format >= 1)
    {
        status = mmal_component_create("vc.ril.video_render", &render);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to create render");
            return -1;
        }
/*
        status = mmal_component_create("vc.ril.isp", &isp);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to create isp");
            return -1;
        }
*/
        status = mmal_component_create("vc.ril.video_splitter", &splitter);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to create splitter");
            return -1;
        }
        status = mmal_component_create("vc.ril.video_encode", &encoder);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to create encoder");
            return -1;
        }
    }
    output = rawcam->output[0];
    if(format >= 1)
    {
/*
        isp_input = isp->input[0];
        isp_output = isp->output[0];
*/
        encoder_input = encoder->input[0];
        encoder_output = encoder->output[0];
        input = render->input[0];
    }
    //if(format == 0)
    {
        
        if(format == 0)
        {
            status = mmal_port_parameter_get(output, &rx_cfg.hdr);
            if(status != MMAL_SUCCESS)
            {
                vcos_log_error("Failed to get cfg");
                goto component_destroy;
            }
            rx_cfg.unpack = UNPACK;
            rx_cfg.pack = PACK;

            vcos_log_error("Set pack to %d, unpack to %d", rx_cfg.unpack, rx_cfg.pack);
            status = mmal_port_parameter_set(output, &rx_cfg.hdr);
            if(status != MMAL_SUCCESS)
            {
                vcos_log_error("Failed to set cfg");
                goto component_destroy;
            }
        }

        else if(format >= 1)
        {

            vcos_log_error("CSI IMAGE ID %x, lanes %d, embedded data lines %d unpack %d pack %d", rx_cfg.image_id, rx_cfg.data_lanes, rx_cfg.embedded_data_lines, rx_cfg.unpack, rx_cfg.pack);
            rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_8;//UNPACK;
            rx_cfg.pack = PACK;//MMAL_CAMERA_RX_CONFIG_PACK_8;//PACK;

            rx_cfg.image_id = 0x2B;

            rx_cfg.image_id = CSI_IMAGE_ID;
            rx_cfg.data_lanes = CSI_DATA_LANES;
            rx_cfg.data_lanes  = 2;

            rx_cfg.embedded_data_lines = 128;//2400;

            vcos_log_error("Set pack to %d, unpack to %d", rx_cfg.unpack, rx_cfg.pack);
            status = mmal_port_parameter_set(output, &rx_cfg.hdr);
            if(status != MMAL_SUCCESS)
            {
                vcos_log_error("Failed to set cfg");
                goto component_destroy;
            }

        }
     
        
    }

    status = mmal_component_enable(rawcam);
    if(status != MMAL_SUCCESS)
    {
        vcos_log_error("Failed to enable");
        goto component_destroy;
    }
    status = mmal_port_parameter_set_boolean(output, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
    if(status != MMAL_SUCCESS)
    {
        vcos_log_error("Failed to set zero copy");
        goto component_disable;
    }
    if(format >= 1)
    {
/*
        vcos_log_error("Enable ISP... ");
        status = mmal_component_enable(isp);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to enable");
            goto component_destroy;
        }
        status = mmal_port_parameter_set_boolean(isp_output, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to set zero copy");
            goto component_disable;
        }
*/
        vcos_log_error("Enable splitter...");
        status = mmal_component_enable(splitter);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to enable");
            goto component_destroy;
        }
        status = mmal_port_parameter_set_boolean(splitter->output[0], MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to set zero copy");
            goto component_disable;
        }

    }

    if(format == 1)
    {
        start_camera_streaming();
        set_size_v2();
    }

    output->format->es->video.crop.width = width;
    output->format->es->video.crop.height = height;
    output->format->es->video.width = VCOS_ALIGN_UP(width, 32);
    output->format->es->video.height = VCOS_ALIGN_UP(height, 16);
    int alignedWidth = output->format->es->video.width;
    int alignedHeight = output->format->es->video.height;
    if(format >= 1)
    {
        output->format->es->video.frame_rate.num = 25;//10000;
        output->format->es->video.frame_rate.den = 1;//10000;
        output->format->encoding = MMAL_ENCODING_RGB24;//MMAL_ENCODING_OPAQUE;//MMAL_ENCODING_RGB24;
//        output->format->encoding_variant = MMAL_ENCODING_I420;//MMAL_ENCODING_RGB24;
    }
    else
    {
        vcos_log_error("Calculated width %d height %d",output->format->es->video.width, output->format->es->video.height);
        output->format->encoding = ENCODING;
    }
    status = mmal_port_format_commit(output);
    if(status != MMAL_SUCCESS)
    {
        vcos_log_error("Failed port_format_commit");
        goto component_disable;
    }
    if(format >= 1)
    {
        output->buffer_size = output->buffer_size_recommended;
        if(format == 1)
            output->buffer_num = 3;
        else
            output->buffer_num = output->buffer_num_recommended;//3;
        vcos_log_error("output: buffer size is %d bytes, num %d", output->buffer_size, output->buffer_num);
/*
        vcos_log_error("create connection rawcam output to isp input...");

        status = mmal_connection_create(&connection[0], output, isp_input, MMAL_CONNECTION_FLAG_TUNNELLING);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to create connection status %d: rawcam->isp", status);
            goto component_disable;
        }
        mmal_format_copy(isp_output->format, isp_input->format);
        isp_output->format->encoding = MMAL_ENCODING_I420;
        vcos_log_error("Setting isp output port format");
        status = mmal_port_format_commit(isp_output);
        isp_output->buffer_size = isp_output->buffer_size_recommended;
        isp_output->buffer_num = isp_output->buffer_num_recommended;
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to create connection status %d: rawcam->isp", status);
            goto component_disable;
        }
*/
        vcos_log_error("Create connection isp output to splitter input...");
        status = mmal_connection_create(&connection[0], output, splitter->input[0], MMAL_CONNECTION_FLAG_TUNNELLING);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to create connection status %d: splitter->render", status);
            goto component_disable;
        }

        vcos_log_error("Create connection splitter output to render input...");
        status = mmal_connection_create(&connection[1], splitter->output[0], input, MMAL_CONNECTION_FLAG_TUNNELLING);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to create connection status %d: splitter->render", status);
            goto component_disable;
        }

        vcos_log_error("Create connection splitter output2 to encoder input...");
        status = mmal_connection_create(&connection[2], splitter->output[1], encoder_input, MMAL_CONNECTION_FLAG_TUNNELLING);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to create connection status %d: splitter->encoder", status);
            goto component_disable;
        }

        if(format == 1)
        {
            encoder_output->format->encoding = MMAL_ENCODING_H264;
            //encoder_output->format->level = MMAL_VIDEO_LEVEL_H264_4;
            encoder_output->format->bitrate = 3000000;//MAX_BITRATE_LEVEL4;//17000000;
            encoder_output->buffer_size = encoder_output->buffer_size_recommended;
            encoder_output->buffer_num = encoder_output->buffer_num_recommended;//8;
        }
        else if(format == 2)
        {
            encoder_output->format->encoding = MMAL_ENCODING_MJPEG;//MMAL_ENCODING_H264;
            encoder_output->format->bitrate = 3000000;
            encoder_output->buffer_size = 256<<10;//encoder_output->buffer_size_recommended;
            encoder_output->buffer_num = encoder_output->buffer_num_recommended;//8;
        }
        if(encoder_output->buffer_size < encoder_output->buffer_size_min)
            encoder_output->buffer_size = encoder_output->buffer_size_min;

        if(encoder_output->buffer_num < encoder_output->buffer_num_min)
            encoder_output->buffer_num = encoder_output->buffer_num_min;
        encoder_output->format->es->video.frame_rate.num = 25;//fps;
        encoder_output->format->es->video.frame_rate.den = 1;

        status = mmal_port_format_commit(encoder_output);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set format on encoder output port");
        }
        if(format == 1)
        {
            MMAL_PARAMETER_VIDEO_PROFILE_T param;
            param.hdr.id = MMAL_PARAMETER_PROFILE;
            param.hdr.size = sizeof(param);

            param.profile[0].profile = MMAL_VIDEO_PROFILE_H264_HIGH;
            param.profile[0].level = MMAL_VIDEO_LEVEL_H264_4;

            status = mmal_port_parameter_set(encoder_output, &param.hdr);
            if(status != MMAL_SUCCESS)
            {
                vcos_log_error("Unabled to set h264 profile");
            }
        }

        if(mmal_port_parameter_set_boolean(encoder_input, MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT, 1) != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set immutable input flag");
        }
        if(mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER, 0) != MMAL_SUCCESS)
        {
            vcos_log_error("failed to set INLINE HEADER FLAG parameters");
        }
        
        if(format == 1 && mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_VECTORS, 0) != MMAL_SUCCESS)
        {
            vcos_log_error("failed to set INLINE VECTORS parameters");
        }

        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set format on video encoder input port");
        }

        vcos_log_error("Enable encoder....");

        status = mmal_component_enable(encoder);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to enable");
            goto component_destroy;
        }
        status = mmal_port_parameter_set_boolean(encoder_output, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to set zero copy");
            goto component_disable;
        }

        vcos_log_error("rawcam supported encodings");
        display_supported_encodings(output);
        vcos_log_error("splitter input supported encodings");
        display_supported_encodings(splitter->input[0]);
        vcos_log_error("splitter output supported encodings");
        display_supported_encodings(splitter->output[0]);

/*
        vcos_log_error("isp input supported encodings");
        display_supported_encodings(isp_input);
        vcos_log_error("isp output supported encodings");
        display_supported_encodings(isp_output);
*/
        vcos_log_error("encoder input supported encodings");
        display_supported_encodings(encoder_input);
        vcos_log_error("encoder output supported encodings");
        display_supported_encodings(encoder_output);
        vcos_log_error("render supported encodings");
        display_supported_encodings(input);

        status == mmal_port_parameter_set_boolean(input, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to set zero copy on video_render");
            goto component_disable;
        }
        if(status == MMAL_SUCCESS)
        {
            vcos_log_error("Enable connection[0]...");
            vcos_log_error("buffer size is %d bytes, num %d", output->buffer_size, output->buffer_num);
            status = mmal_connection_enable(connection[0]);
            if(status != MMAL_SUCCESS)
            {
                mmal_connection_destroy(connection[0]);
            }
/*
            vcos_log_error("Enable connection[1]...");
            vcos_log_error("buffer size is %d bytes, num %d", isp_output->buffer_size, isp_output->buffer_num);
            status = mmal_connection_enable(connection[1]);
            if(status != MMAL_SUCCESS)
            {
                mmal_connection_destroy(connection[1]);
            }
*/
            vcos_log_error("Enable connection[1]...");
            vcos_log_error("buffer size is %d bytes, num %d", splitter->output[0]->buffer_size, splitter->output[0]->buffer_num);
            status = mmal_connection_enable(connection[1]);
            if(status != MMAL_SUCCESS)
            {
                mmal_connection_destroy(connection[1]);
            }
            vcos_log_error("Enable connection[2]....");
            vcos_log_error("buffer size is %d bytes, num %d", splitter->output[1]->buffer_size, splitter->output[1]->buffer_num);
            status = mmal_connection_enable(connection[2]);
            if(status != MMAL_SUCCESS)
            {
                mmal_connection_destroy(connection[2]);
            }
        }
        //encoder_output->userdata = (void*)open_filename("test_encode.h264");
        vcos_log_error("Create pool of %d buffers of size %d", encoder_output->buffer_num, encoder_output->buffer_size);
        pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);
        if(!pool)
        {
            vcos_log_error("Failed to create pool");
            goto component_disable;
        }
        status = mmal_port_enable(encoder_output, encoder_buffer_callback);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to enable port");
        }
       // running = 1;
        int i;
        for(i = 0 ; i < encoder_output->buffer_num ; i++)
        {
            MMAL_BUFFER_HEADER_T * buffer = mmal_queue_get(pool->queue);
            if(!buffer)
            {
                vcos_log_error("Where'd my buffer go?");
                goto port_disable;
            }
            status = mmal_port_send_buffer(encoder_output, buffer);
            if(status != MMAL_SUCCESS)
            {
                vcos_log_error("mmal_port_send_buffer failed on buffer %p, status %d", buffer, status);
                goto port_disable;
            }
            vcos_log_error("Sent buffer %p", buffer);
        }
        vcos_log_error("All done, start streaming...");
        
    }
    else
    {
        vcos_log_error("width %d height %d",output->format->es->video.width, output->format->es->video.height);
        vcos_log_error("Create pool of %d buffers of size %d", output->buffer_num, output->buffer_size);
        fprintf(stdout, "AlignedSize:%d:%d:%d:%d:%d\n", output->buffer_size, output->format->es->video.width, output->format->es->video.height,alignedWidth,alignedHeight);
        fflush(stdout);
        pool = mmal_port_pool_create(output, output->buffer_num, output->buffer_size);
        if(!pool)
        {
            vcos_log_error("Failed to create pool");
            goto component_disable;
        }
        
        if(previewMode == 0)
            status = mmal_port_enable(output, callback);
        else
            status = mmal_port_enable(output, preview_callback);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to enable port");
            goto pool_destroy;
        }
    }
    running = 1;

    if(format == 0)
    {
        for(i=0; i<output->buffer_num; i++)
        {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);
            
            if (!buffer)
            {
                vcos_log_error("Where'd my buffer go?!");
                goto port_disable;
            }
            status = mmal_port_send_buffer(output, buffer);
            if(status != MMAL_SUCCESS)
            {
                vcos_log_error("mmal_port_send_buffer failed on buffer %p, status %d", buffer, status);
                goto port_disable;
            }
            vcos_log_error("Sent buffer %p", buffer);
        }
    }
    //Setup GPIO for camera and recording status
    setup_io();
    changeStatusOfCam(0);
    changeStatusOfRecording(0);
    stop_camera_streaming();
    char ch;
    /******** Recording Mode *********/
    if(previewMode == 0)
    {
        if(pre_alloc)
        {
            allocateNode(output->buffer_size);
        }
        if(format == 0)
        {
            start_camera_streaming();
            set_size_v2();
        }
        changeStatusOfCam(1);
        
        vcos_log_error("Wait for input to start recording\n");
        ch = getchar();
        //vcos_log_error("1\n");
        
        mmal_port_parameter_get_uint64(output, MMAL_PARAMETER_SYSTEM_TIME, &record_start_pressed_time);
        wake_up();
        //vcos_log_error("2\n");
        /*
         if(use_write_thread && pre_alloc == 0 && devide_buffer_by == 0)
         {
         if(pthread_create(&write_thread, NULL, write_queued_buffer, NULL))
         {
         printf("Failed to create write_thread\n");
         }
         write_thread_create = 1;
         }
         */
        if(format == 0 && use_write_thread)
        {
            if(pthread_create(&write_thread, NULL, write_one_queued_buffer, NULL))
            {
                printf("Failed to create write_thread\n");
            }
            write_thread_create = 1;
        }
        //vcos_log_error("3\n");
        changeStatusOfRecording(1);
        //vcos_sleep(30000);
        ch = getchar();
        //vcos_log_error("4\n");
        running = 0;
        stop_camera_streaming();
        changeStatusOfRecording(0);
        printf("Recording status change to 0\n");
        if(format == 0)
        {
            if (pre_alloc || devide_buffer_by > 0)
            {
                pthread_cond_signal(&condAvail);
                pthread_join(write_thread, NULL);
                //write_buffer_node_chain();
            }
            else
            {
                if(use_write_thread)
                {
                    pthread_cond_signal(&condAvail);
                }
                if(write_thread_create && buffer_delivered)
                {
                    pthread_join(write_thread, NULL);
                }
            }
        }        
        if(dfile)
        {
            close(dfile);
        }
        if(ofile)
        {
            fclose(ofile);
        }
        if(dumfile)
        {
            fclose(dumfile);
        }
        printf("Files closed\n");
    }
    else
    {
        /******** Start Preview Mode *********/
        start_camera_streaming();
        set_size_v2();
        changeStatusOfCam(1);
        wake_up();
        ch = getchar();
        running = 0;
        stop_camera_streaming();
        changeStatusOfRecording(0);
        /******** End Preview Mode *********/
    }
    printf("Wait for key input\n");
    ch = getchar();
    printf("Last key entered\n");
    changeStatusOfCam(0);
    printf("Cam status changed to 0\n");
port_disable:
    if(format == 1)
    {
        mmal_connection_disable(connection[0]);
        mmal_connection_destroy(connection[0]);

        mmal_connection_disable(connection[1]);
        mmal_connection_destroy(connection[1]);

        mmal_connection_disable(connection[2]);
        mmal_connection_destroy(connection[2]);
/*
        mmal_connection_disable(connection[3]);
        mmal_connection_destroy(connection[3]);
*/
    }
    else
    {
        status = mmal_port_disable(output);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to disable port");
            return -1;
        }
    }
pool_destroy:
    if(format == 0)
    {
        vcos_log_error("pool destroy");
        mmal_port_pool_destroy(output, pool);
    }
component_disable:
    vcos_log_error("component disable");
    status = mmal_component_disable(rawcam);
    if(status != MMAL_SUCCESS)
    {
        vcos_log_error("Failed to disable");
    }
    if(format == 1)
        {
/*
        status = mmal_component_disable(isp);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to disable isp");
        }
*/
        status = mmal_component_disable(render);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to disable render");
        }
        status = mmal_component_disable(splitter);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to disable splitter");
        }
        status = mmal_component_disable(encoder);
        if(status != MMAL_SUCCESS)
        {
            vcos_log_error("Failed to disable encoder");
        }
    }

component_destroy:
    vcos_log_error("component destroy");
    mmal_component_destroy(rawcam);
    vcos_log_error("component rawcam destroyed");
    if(format == 1)
    {
//        mmal_component_destroy(isp);
//        vcos_log_error("component isp destroyed");
        mmal_component_destroy(splitter);
        vcos_log_error("component splitter destroyed");
        mmal_component_destroy(render);
        vcos_log_error("component render destroyed");
        mmal_component_destroy(encoder);
        vcos_log_error("component encoder destroyed");

    }
    vcos_log_error("exit");
    return 0;
}
//
// Set up a memory regions to access GPIO
//

void changeStatusOfRecording(int recording)
{
    
    if(recording)
    {
        GPIO_SET = 1<<GPIO_CAMERA_RECORDING_OPERATION;
    }
    else
    {
        GPIO_CLR = 1<<GPIO_CAMERA_RECORDING_OPERATION;
    }
    fprintf(stdout, "changeStatusOfRecording %d \n", GET_GPIO(GPIO_CAMERA_RECORDING_OPERATION));
    fflush(stdout);
    //printButton(GPIO_CAMERA_RECORDING_OPERATION);
}

void changeStatusOfCam(int on)
{
    if(on)
    {
        GPIO_SET = 1<<GPIO_CAMERA_OPERATION;
    }
    else
    {
        GPIO_CLR = 1<<GPIO_CAMERA_OPERATION;
        GPIO_CLR = 1<<GPIO_CAMERA_RECORDING_OPERATION;
    }
    //printButton(GPIO_CAMERA_OPERATION);
}

void setup_io()
{
    /* open /dev/mem */
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
        printf("can't open /dev/mem \n");
        exit(-1);
    }
    
    /* mmap GPIO */
    gpio_map = mmap(
                    NULL,             //Any adddress in our space will do
                    BLOCK_SIZE,       //Map length
                    PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
                    MAP_SHARED,       //Shared with other processes
                    mem_fd,           //File to map
                    GPIO_BASE         //Offset to GPIO peripheral
                    );
    
    close(mem_fd); //No need to keep mem_fd open after mmap
    
    if (gpio_map == MAP_FAILED) {
        printf("mmap error %d\n", (int)gpio_map);//errno also set!
        exit(-1);
    }
    
    // Always use volatile pointer!
    gpio = (volatile unsigned *)gpio_map;
    
    OUT_GPIO(GPIO_CAMERA_RECORDING_OPERATION);
    OUT_GPIO(GPIO_CAMERA_OPERATION);
    
} // setup_io


int allocateNode(int buffer_size)
{
    
    fprintf(stdout, "allocate pre\n");
    struct FLUX_BUFFERNODE * node = (struct FLUX_BUFFERNODE *)malloc(sizeof(struct FLUX_BUFFERNODE));
    if(devide_buffer_by > 0)
    {
        buffer_size = buffer_size/devide_buffer_by;
    }
    node->data = (uint8_t*)malloc(buffer_size);
    node->next = 0;
    node->index = -1;
    int i;
    struct FLUX_BUFFERNODE * buffer_node = node;
    nextBufferNode = node;
    firstBufferNode = node;
    int node_count = total_frame_count;
    if(devide_buffer_by > 0)
    {
        node_count = node_count * devide_buffer_by;
    }
    for(i = 1 ; i < node_count ; i++)
    {
        buffer_node->next = (struct FLUX_BUFFERNODE *)malloc(sizeof(struct FLUX_BUFFERNODE));
        buffer_node = buffer_node->next;
        buffer_node->data = (uint8_t*)malloc(buffer_size);
        buffer_node->index = -1;
    }
    fprintf(stdout, "Allocate Node: created %d nodes as devided by %d\n", i, devide_buffer_by);
    lastBufferNode = buffer_node;
    return 0;
}
void compareWithFirst(MMAL_BUFFER_HEADER_T *buffer)
{
    int index;
    int diff=0;
    for(index = 0 ; index < buffer->length ; index++)
    {
        if(buffer->data[index]!=firstBufferNode->data[index])
            diff++;
    }
    fprintf(stdout, "Compared diff count = %d\n", diff);
    fflush(stdout);
}
void add_buffer_to_node_chain(FILE * file_handle, MMAL_BUFFER_HEADER_T *buffer, MMAL_PORT_T *port) {
    //  buffer_delivered = 1;
    /*
     if(buffer_queue_index > 0)
     {
     compareWithFirst(buffer);
     }
     */
    //int length = fwrite(buffer->data, sizeof(uint8_t), buffer->length/4, dumfile);
    //fprintf(stdout, "add buffer to node chaing wrote %d\n", length);
    //fflush(stdout);
    memcpy(nextBufferNode->data, buffer->data, buffer->length);
    nextBufferNode->index = buffer_queue_index;
    nextBufferNode->length = buffer->length;
    //fprintf(stdout, "add_buffer_to_node_chain cp data index %d\n", nextBufferNode->index);
    //fflush(stdout);
    nextBufferNode = nextBufferNode->next;
    
    buffer_queue_index++;
    if(buffer_queue_index >= total_frame_count)
    {
        fprintf(stdout, "ReachedMaximumFrames\n");
        fflush(stdout);
    }
}

void add_buffer_to_node_chain_devided(FILE * file_handle, MMAL_BUFFER_HEADER_T *buffer, MMAL_PORT_T *port) {
    int index = 0;
    int buffer_length = buffer->length/devide_buffer_by;
    for(index = 0 ; index < devide_buffer_by ; index++)
    {
        memcpy(nextBufferNode->data, &buffer->data[index*buffer_length], buffer_length);
        nextBufferNode->index = buffer_queue_index;
        nextBufferNode->length = buffer_length;
        fprintf(stdout, "add_buffer_to_node_chain cp data index %d (%d)\n", nextBufferNode->index, index);
        //fflush(stdout);
        nextBufferNode = nextBufferNode->next;
    }
    buffer_queue_index++;
    if(buffer_queue_index >= total_frame_count)
    {
        fprintf(stdout, "ReachedMaximumFrames\n");
        fflush(stdout);
    }
}
void cpyBuffer8Bit(uint8_t * dst, uint8_t * src, int len)
{
    fprintf(stdout, "cpyBuffer %i len<<<\n", len);
    int index = 0;
    int srcIndex = 0;
    uint64_t beforetime = vcos_getmicrosecs64()/1000;
    for(index = 0 ; index < len ; index+=5)
    {
        //fprintf(stdout, "cpyBuffer index %i\n", index);
        memcpy(&dst[srcIndex], &src[index], 4);
        srcIndex+=4;
    }
    uint64_t aftertime = vcos_getmicrosecs64()/1000;
    fprintf(stdout, "cpyBuffer took %lld>>> \n", (aftertime-beforetime));
}
void add_buffer_node_divided(FILE * file_handle, MMAL_BUFFER_HEADER_T *buffer, MMAL_PORT_T *port)
{
    int index = 0;
    int buffer_length = buffer->length/devide_buffer_by;
    int target_buf_length;
    if(encodingBit == 8)
        target_buf_length = width*height/devide_buffer_by;
    else
        target_buf_length = buffer_length;
    fprintf(stdout, "add_buffer_node_divided buffer length = %d\n", buffer_length);
    //fflush(stdout);
    uint64_t beforetime;
    mmal_port_parameter_get_uint64(port, MMAL_PARAMETER_SYSTEM_TIME, &beforetime);
    for(index = 0 ; index < devide_buffer_by ; index++)
    {
        struct FLUX_BUFFERNODE * node;
        if(firstfreedNode)
        {
            fprintf(stdout, "get from freed node list\n");
            node = firstfreedNode;
            if(firstfreedNode == lastfreedNode)
            {
                firstfreedNode = NULL;
                lastfreedNode = NULL;
            }
            else
            {
                firstfreedNode = firstfreedNode->next;
            }
        }
        else
        {
            fprintf(stdout, "allocate new node\n");
            node = (struct FLUX_BUFFERNODE *)malloc(sizeof(struct FLUX_BUFFERNODE));
            node->data = (uint8_t*)malloc(target_buf_length);
        }
        node->file_handle = file_handle;
        node->length = target_buf_length;
        node->index = node_count;
        //fprintf(stdout, "Buffer data index = %d\n", index*buffer_length);
        //fflush(stdout);
        if(encodingBit == 8)
            cpyBuffer8Bit(node->data, &buffer->data[index*buffer_length], buffer_length);
        else
            memcpy(node->data, &buffer->data[index*buffer_length], buffer_length);
        //fprintf(stdout, "Add devided buffer %d(%d)\n", buffer_queue_index, index);
        //fflush(stdout);
        if(firstBufferNode)
        {
            lastBufferNode->next = node;
        }
        else
        {
            firstBufferNode = node;
        }
        lastBufferNode = node;
        node_count++;
    }
    buffer_queue_index++;
    uint64_t aftertime;
    mmal_port_parameter_get_uint64(port, MMAL_PARAMETER_SYSTEM_TIME, &aftertime);
    //fprintf(stdout, "add bufer_node devidied took %lld\n", (aftertime-beforetime)/1000);
    //fflush(stdout);
    
    
    if(buffer_queue_index >= total_frame_count)
    {
        fprintf(stdout, "ReachedMaximumFrames\n");
        fflush(stdout);
    }
}
void write_first_node()
{
    /**
     * Write first node data to file
     */
    if(firstBufferNode && firstBufferNode->index != -1)
    {
        int length = write(dfile, firstBufferNode->data, firstBufferNode->length);
        //int length = fwrite(firstBufferNode->data, sizeof(uint8_t), firstBufferNode->length, ofile);
        fprintf(stdout, "write frame (%d) %d\n", firstBufferNode->index, length);
        struct FLUX_BUFFERNODE * curNode = firstBufferNode;
        /*
         if(firstBufferNode == lastBufferNode)
         {
         break;
         }
         */
        if(firstBufferNode->next)
        {
            firstBufferNode = firstBufferNode->next;
        }
        else
        {
            firstBufferNode = NULL;
        }
        free(curNode->data);
        free(curNode);
    }
}
/**
 * It copies buffer arrived from VCHIQ and keeps in the form of linked list on RAM.
 * condAvail signal will be sent to write_queued_buffer as soon as first 5 buffers stored.
 */
void add_buffer_node(FILE * file_handle, MMAL_BUFFER_HEADER_T *buffer, MMAL_PORT_T *port) {
    
    
    buffer_delivered = 1;
    
    int64_t start_time = vcos_getmicrosecs64()/1000;
    // Create new buffer node by allocating and copying memory to keep the list on RAM
    struct FLUX_BUFFERNODE * node = (struct FLUX_BUFFERNODE *)malloc(sizeof(struct FLUX_BUFFERNODE));
    
    int64_t malloc_time = vcos_getmicrosecs64()/1000 - start_time;
    node->file_handle = file_handle;
    node->data = (uint8_t*)malloc(buffer->length);
    
    //memcpy(node->data, data, length);
    memcpy(node->data, buffer->data, buffer->length);
    //memmove(node->data, data, length);
    
    
    //buffer->length = 0;
    //mmal_port_send_buffer(port, buffer);
    int64_t copy_time = vcos_getmicrosecs64()/1000 - start_time;
    
    node->index = buffer_queue_index;
    node->length = buffer->length;
    node->next = 0;
    buffer->length = 0;
    fprintf(stdout, "buffer(%p) to node(%p) data(%p) index(%d)\n", buffer, node, node->data, node->index);
    mmal_port_send_buffer(port, buffer);
    
    if(firstBufferNode)
    {
        lastBufferNode->next = node;
    }
    else
    {
        firstBufferNode = node;
    }
    lastBufferNode = node;
    buffer_size++;
    buffer_queue_index++;
    
    if(buffer_queue_index >= total_frame_count)
    {
        fprintf(stdout, "ReachedMaximumFrames\n");
        fflush(stdout);
    }
    //Wait until there are 5 buffers stored on RAM not to interfere camera thread
    // Then gives signal to write_queued_buffer thread
    if((buffer_size == 5 && writing_file_started == 0) ||
       (buffer_size > 0 && writing_file_started == 1 && waiting_for_frame_buffer == 1))
    {
        waiting_for_frame_buffer = 0;
        pthread_cond_signal(&condAvail);
        writing_file_started = 1;
    }
    int64_t end_time = vcos_getmicrosecs64()/1000 - start_time;
    fprintf(stdout, "add_buffer_node (%d) malloc(%lld) copy (%lld) total(%lld)\n", buffer_queue_index, malloc_time, copy_time, end_time);
    fflush(stdout);
    
}

/**
 *  Returns most front buffer node
 */
struct FLUX_BUFFERNODE * get_front_buffer_node()
{
    struct FLUX_BUFFERNODE * node = firstBufferNode;
    /*
     if(node)
     {
     firstBufferNode = node->next;
     }
     */
    return node;
}

void write_buffer_node_chain()
{
    struct FLUX_BUFFERNODE * firstNode = firstBufferNode;
    while(1){
        if(firstBufferNode && firstBufferNode->index != -1)
        {
            int length = write(dfile, firstBufferNode->data, firstBufferNode->length);
            //      int length = fwrite(firstBufferNode->data, sizeof(uint8_t), firstBufferNode->length, ofile);
            fprintf(stdout, "write frame (%d) %d\n", firstBufferNode->index, length);
            struct FLUX_BUFFERNODE * curNode = firstBufferNode;
            
            if(firstBufferNode == lastBufferNode)
            {
                break;
            }
            if(firstBufferNode->next)
            {
                firstBufferNode = firstBufferNode->next;
            }
            free(curNode->data);
            free(curNode);
        }
    }
    
    fprintf(stdout, "write completed\n");
    fflush(stdout);
}
/**
 * This function runs on a separate pthread
 * gets frame buffers stored on RAM by add_buffer_node function
 * If there is no queued buffer, it will wait for signal (condAvail) that will be sent by add_buffer_node function
 */
void * write_one_queued_buffer() {
    while(1) {
        struct FLUX_BUFFERNODE * node = get_front_buffer_node();
/*
        if(node->next)
        {
            firstBufferNode = node->next;
        }
        else
        {
            firstBufferNode = NULL;
            lastBufferNode = NULL;
        }
*/
        if(node)
        {
            //printf("write buffer %d\n", node->index);
            int64_t start_time = vcos_getmicrosecs64()/1000;
            char filename[50];
            //sprintf(filename, "%s%d.raw", directory, node->index);
            //FILE *file;
            //file = fopen(filename, "wb");
            //int64_t start2_time = vcos_getmicrosecs64()/1000 - start_time;
            //int length = fwrite(node->data, sizeof(uint8_t), node->length, ofile);
            int length = write(dfile, node->data, node->length);
            //int length = fwrite(node->data, sizeof(uint8_t), node->length, file);
            //fclose(file);
            int64_t end_time = vcos_getmicrosecs64()/1000 - start_time;
            fprintf(stdout, "write_one_queued_buffer node(%p) buffer(%p) %d took (%lld)\n", node, node->data, node->index, end_time);
            fflush(stdout);
            free(node->data);
            firstBufferNode = node->next;
            free(node);
            
/*
            if(firstfreedNode)
            {
                fprintf(stdout, "Add to last freednode list\n");
                lastfreedNode->next = node;
                lastfreedNode = node;
            }
            else
            {
                fprintf(stdout, "Add to as first freednode list\n");
                firstfreedNode = node;
                lastfreedNode = node;
            }
            //free(node->data);
            //free(node);
*/
            buffer_size--;
            firstBufferNode = node->next;
        }
        //   else
        //   {
        //If there is no buffer is queue and writing file process started and recording GPIO is off
        // It will exit meaning recording process finished and all buffers are written to file.
        // writing_file_started flag is used as sometimes recording gpio pin is not up yet when first buffer arrived
        // causing early exit at first call before first buffer arrives.
        if(running == 0 || GET_GPIO(GPIO_CAMERA_RECORDING_OPERATION) == 0)
        {
            //Wait for 1second and check again if there is no buffer anymore
            //vcos_sleep(1000);
            if(get_front_buffer_node() == NULL){
                fprintf(stdout, "write completed\n");
                fflush(stdout);
                break;
            }
        }
        else
        {
            fprintf(stdout, "write_queued_buffer wait for signal gpio %d writing_file_started = %d\n", GET_GPIO(GPIO_CAMERA_RECORDING_OPERATION), writing_file_started);
            fflush(stdout);
            waiting_for_frame_buffer = 1;
            pthread_mutex_lock(&mutex);
            pthread_cond_wait(&condAvail, &mutex);
            pthread_mutex_unlock(&mutex);
            
        }
        //    }
    }
}
/**
 * This function runs on a separate pthread
 * gets frame buffers stored on RAM by add_buffer_node function
 * If there is no queued buffer, it will wait for signal (condAvail) that will be sent by add_buffer_node function
 */
void * write_queued_buffer() {
    while(1) {
        struct FLUX_BUFFERNODE * node = get_front_buffer_node();
        if(node)
        {
            //printf("write buffer %d\n", node->index);
            int64_t start_time = vcos_getmicrosecs64()/1000;
            char filename[50];
            //sprintf(filename, "%s%d.raw", directory, node->index);
            //FILE *file;
            //file = fopen(filename, "wb");
            int64_t start2_time = vcos_getmicrosecs64()/1000 - start_time;
            //       int length = fwrite(node->data, sizeof(uint8_t), node->length, ofile);
            int length = write(dfile, node->data, node->length);
            //int length = fwrite(node->data, sizeof(uint8_t), node->length, file);
            //fclose(file);
            int64_t end_time = vcos_getmicrosecs64()/1000 - start_time;
            fprintf(stdout, "write node(%p) buffer(%p) %d took (%lld)(%lld)\n", node, node->data, node->index, start2_time, end_time);
            fflush(stdout);
            firstBufferNode = node->next;
            free(node->data);
            free(node);
            buffer_size--;
        }
        else
        {
            //If there is no buffer is queue and writing file process started and recording GPIO is off
            // It will exit meaning recording process finished and all buffers are written to file.
            // writing_file_started flag is used as sometimes recording gpio pin is not up yet when first buffer arrived
            // causing early exit at first call before first buffer arrives.
            if(GET_GPIO(GPIO_CAMERA_RECORDING_OPERATION) == 0 && writing_file_started == 1)
            {
                //Wait for 1second and check again if there is no buffer anymore
                vcos_sleep(1000);
                if(get_front_buffer_node() == NULL){
                    fprintf(stdout, "write completed\n");
                    fflush(stdout);
                    break;
                }
            }
            else
            {
                fprintf(stdout, "write_queued_buffer wait for signal gpio %d writing_file_started = %d\n", GET_GPIO(GPIO_CAMERA_RECORDING_OPERATION), writing_file_started);
                fflush(stdout);
                waiting_for_frame_buffer = 1;
                pthread_mutex_lock(&mutex);
                pthread_cond_wait(&condAvail, &mutex);
                pthread_mutex_unlock(&mutex);
                
            }
        }
    }
}

void* flux_memcpy(void* dest, const void* src, size_t count)
{
    
    char* dst8 = (char*)dest;
    char* src8 = (char*)src;
    
    while(count--)
    {
        *dst8++ = *src8++;
    }
    return dest;
}
void* flux_memcpy2(void* dest, const void* src, size_t count)
{
    
    char* dst8 = (char*)dest;
    char* src8 = (char*)src;
    
    if(count & 1)
    {
        dst8[0] = src8[0];
        dst8 +=1;
        src8 += 1;
    }
    count /= 2;
    while(count--)
    {
        dst8[0] = src8[0];
        dst8[1] = src8[1];
        
        dst8 += 2;
        src8 += 2;
    }
    return dest;
}
void* flux_memcpy3(void* dest, const void* src, size_t count)
{
    char* dst8 = (char*)dest;
    char* src8 = (char*)src;
    
    --src8;
    --dst8;
    while(count--)
    {
        *++dst8 = *++src8;
    }
    return dest;
}


void checkVersion()
{
    FILE *cpuinfo = fopen("/proc/cpuinfo","rb");
    char *arg = 0;
    size_t size = 0;
    while(getdelim(&arg, &size, 0, cpuinfo) != -1)
    {
        char * pch;
        pch = strtok(arg, "\n");
        while (pch != NULL)
        {
            //fprintf(stdout, "Line : %s\n", pch);
            if(strstr(pch, "Revision") != NULL){
                char * rev = strtok(pch, " : ");
                //fprintf(stdout, "### rev 1: %s\n", rev);
                //fflush(stdout);
                rev = strtok(NULL, " : ");
                //fprintf(stdout, "### rev 2: %s\n", rev);
                //fflush(stdout);
                if(strcmp(rev, PI_REVISION_2_1) == 0 || strcmp(rev, PI_REVISION_2_2) == 0 || strcmp(rev, PI_REVISION_2_3) == 0 || strcmp(rev, PI_REVISION_2_4) == 0)
                {
                    i2c_file = "/dev/i2c-0";
                } 
                else if(strcmp(rev, PI_REVISION_3_1) == 0 || strcmp(rev, PI_REVISION_3_2) == 0 || strcmp(rev, PI_REVISION_3_3) == 0 || strcmp(rev, PI_REVISION_3_4) == 0)
                {
                    i2c_file = "/dev/i2c-1";
                }
                fprintf(stdout, "i2c device file %s\n", i2c_file);
                return;
                
            }
            pch = strtok(NULL, "\n");
        }
    }
}

#define MAX_ENCODINGS_NUM 20
typedef struct {
    MMAL_PARAMETER_HEADER_T header;
    MMAL_FOURCC_T encodings[MAX_ENCODINGS_NUM];
} MMAL_SUPPORTED_ENCODINGS_T;

void display_supported_encodings(MMAL_PORT_T *port)
{
    MMAL_SUPPORTED_ENCODINGS_T sup_encodings = {{MMAL_PARAMETER_SUPPORTED_ENCODINGS, sizeof(sup_encodings)}, {0}};
    if(mmal_port_parameter_get(port, &sup_encodings.header) == MMAL_SUCCESS)
    {
        int i;
        int num_encodings = (sup_encodings.header.size - sizeof(sup_encodings.header)) /
            sizeof(sup_encodings.encodings[0]);
        for(i=0 ; i<num_encodings ; i++)
        {
            switch(sup_encodings.encodings[i])
            {
                case MMAL_ENCODING_I420:
                    vcos_log_error("MMAL_ENCODING_I420");
                    break;
                case MMAL_ENCODING_I420_SLICE:
                    vcos_log_error("MMAL_ENCODING_I420_SLICE");
                    break;
                case MMAL_ENCODING_YV12:
                    vcos_log_error("MMAL_ENCODING_YV12");
                    break;
                case MMAL_ENCODING_I422:
                    vcos_log_error("MMAL_ENCODING_I422");
                    break;
                case MMAL_ENCODING_I422_SLICE:
                    vcos_log_error("MMAL_ENCODING_I422_SLICE");
                    break;
                case MMAL_ENCODING_YUYV:
                    vcos_log_error("MMAL_ENCODING_YUYV");
                    break;
                case MMAL_ENCODING_YVYU:
                    vcos_log_error("MMAL_ENCODING_YVYU");
                    break;
                case MMAL_ENCODING_UYVY:
                    vcos_log_error("MMAL_ENCODING_UYVY");
                    break;
                case MMAL_ENCODING_VYUY:
                    vcos_log_error("MMAL_ENCODING_VYUY");
                    break;
                case MMAL_ENCODING_NV12:
                    vcos_log_error("MMAL_ENCODING_NV12");
                    break;
                case MMAL_ENCODING_NV21:
                    vcos_log_error("MMAL_ENCODING_NV21");
                    break;
                case MMAL_ENCODING_ARGB:
                    vcos_log_error("MMAL_ENCODING_ARGB");
                    break;
                case MMAL_ENCODING_RGBA:
                    vcos_log_error("MMAL_ENCODING_RGBA");
                    break;
                case MMAL_ENCODING_ABGR:
                    vcos_log_error("MMAL_ENCODING_ABGR");
                    break;
                case MMAL_ENCODING_BGRA:
                    vcos_log_error("MMAL_ENCODING_BGRA");
                    break;
                case MMAL_ENCODING_RGB16:
                    vcos_log_error("MMAL_ENCODING_RGB16");
                    break;
                case MMAL_ENCODING_RGB24:
                    vcos_log_error("MMAL_ENCODING_RGB24");
                    break;
                case MMAL_ENCODING_RGB32:
                    vcos_log_error("MMAL_ENCODING_RGB32");
                    break;
                case MMAL_ENCODING_BGR16:
                    vcos_log_error("MMAL_ENCODING_BGR16");
                    break;
                case MMAL_ENCODING_BGR24:
                    vcos_log_error("MMAL_ENCODING_BGR24");
                    break;
                case MMAL_ENCODING_BGR32:
                    vcos_log_error("MMAL_ENCODING_BGR32");
                    break;
                default:
                    vcos_log_error("supported %d", sup_encodings.encodings[i]);
                    break;

            }
        }

    }
    else
    {
        vcos_log_error("Failed to get supported encodings");
    }
}
