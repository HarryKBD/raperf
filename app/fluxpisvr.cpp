#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <time.h>
#include <sys/time.h>
#include <iostream>
#include <udt.h>
#include "cc.h"
#include "test_util.h"
#include "buf.h"
#include "debayer.h"
#include "cairo_display.h"
#include "stopwatch.h"

#define MAX_CLIENT_SIZE 6
using namespace std;

char *recv_buf[MAX_CLIENT_SIZE];
void * recvdata(void *);
void *process_buf(void *id);


//#define RAW_FRAME_SIZE 3686400



int main(int argc, char * argv[])
{
    if ((1 != argc) && ((2 != argc) || (0 == atoi(argv[1]))))
    {
        cout << "usage: appserver [server_port]" << endl;
        return 0;
    }


    init_gtk();

    // Automatically start up and clean up UDT module.
    UDTUpDown _udt_;

    addrinfo hints;
    addrinfo * res;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    //hints.ai_socktype = SOCK_DGRAM;

    string service("9000");

    if (2 == argc)
        service = argv[1];

    if (0 != getaddrinfo(NULL, service.c_str(), &hints, &res))
    {
        cout << "illegal port number or port is busy.\n" << endl;
        return 0;
    }

    UDTSOCKET serv = UDT::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    // UDT Options
    //UDT::setsockopt(serv, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
    //UDT::setsockopt(serv, 0, UDT_MSS, new int(9000), sizeof(int));
    //UDT::setsockopt(serv, 0, UDT_RCVBUF, new int(10000000), sizeof(int));
    //UDT::setsockopt(serv, 0, UDP_RCVBUF, new int(10000000), sizeof(int));

    if (UDT::ERROR == UDT::bind(serv, res->ai_addr, res->ai_addrlen))
    {
        cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }

    freeaddrinfo(res);

    cout << "server is ready at port: " << service << endl;

    if (UDT::ERROR == UDT::listen(serv, 10))
    {
        cout << "listen: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }

    sockaddr_storage clientaddr;
    int addrlen = sizeof(clientaddr);

    UDTSOCKET recver;
    while (true)
    {
        if (UDT::INVALID_SOCK == (recver = UDT::accept(serv, (sockaddr *)&clientaddr, &addrlen)))
        {
            cout << "accept: " << UDT::getlasterror().getErrorMessage() << endl;
            return 0;
        }

        char clienthost[NI_MAXHOST];
        char clientservice[NI_MAXSERV];
        getnameinfo((sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), clientservice, sizeof(clientservice), NI_NUMERICHOST | NI_NUMERICSERV);
        cout << "new connection: " << clienthost << ":" << clientservice << endl;

        FrameBuf *pbuf;

        
        static int thread_idx = 0;

        pbuf = init_buffer(clienthost, thread_idx++);

        pbuf->sd = UDPSOCKET(recver);

        //pbuf->cond = PTHREAD_COND_INITIALIZER;
        //pbuf->mutex = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_init(&pbuf->mutex, NULL);
        pthread_cond_init(&pbuf->cond, NULL);


        GTK_DATA *pDisplay = init_display();
        strcpy(pDisplay->title, pbuf->client_ip);
        pbuf->puser = (void *)pDisplay;

        pthread_t rcvthread;
        pthread_t readthread;

        pthread_create(&readthread, NULL, process_buf, pbuf);
        pthread_create(&rcvthread, NULL, recvdata, pbuf);
        pthread_detach(rcvthread);
        pthread_detach(readthread);
    }

    UDT::close(serv);

    return 0;
}

void *recvdata(void *p)
{

    FrameBuf *pbuf = (FrameBuf *)p;
    UDTSOCKET recver = pbuf->sd;

    char* buf;
    int buf_size = 100000;
    buf = new char[buf_size];
    int rs;
    //int saved_len = 0;
    cout << "Creating recving packet from PI thread" << endl;

    while (1){
        //int var_size = sizeof(int);
        //UDT::getsockopt(recver, 0, UDT_RCVDATA, &rcv_size, &var_size);
        char *save_buf;
        int save_buf_len;

        save_buf = get_save_buf_ptr(pbuf, 100000, &save_buf_len);
        if(save_buf == NULL){
            printf("buf null error\n");
            usleep(1000000);
            continue;
        }
        else{
         //   printf("save buf len %d\n", save_buf_len);
        }

        if (UDT::ERROR == (rs = UDT::recv(recver, save_buf, save_buf_len, 0)))
        {
           cout << "recvdata:" << UDT::getlasterror().getErrorMessage() << endl;
           break;
        }
        if(rs != save_buf_len){
         //   printf("adjust buf to %d\n", save_buf_len - rs);
            adjust_buf_header(pbuf, save_buf_len - rs);
        }
        //start();
        //saved_len = save_data(pbuf, buf, rs);
        //stop("save");
        /*
        if(saved_len != rs){
            printf("Buffer might be full already..\n");
            usleep(5000);
            printf("Starting read again\n");
        }
        */

        if(get_data_cnt(pbuf) > RAW_FRAME_SIZE * 2){
            pthread_cond_signal(&pbuf->cond);
        }
    }
    pbuf->rcv_running = 0;
    delete [] buf;

    cout << "recvdata: receiving is completed." << endl;
    UDT::close(recver);
    return NULL;
}


void *process_buf(void *p){

    cout << "Starting reading frame from buffer" << endl;
    FrameBuf *pbuf = (FrameBuf *)p;
    int data_len = 0;
    int frame_idx = 0;
    GTK_DATA *pDisplay = (GTK_DATA *)pbuf->puser;
    cout << " client ID is : " << pbuf->sd << endl;

    while(pbuf->rcv_running){
        
        pthread_mutex_lock(&pbuf->mutex);
        pthread_cond_wait(&pbuf->cond, &pbuf->mutex);
        pthread_mutex_unlock(&pbuf->mutex);
        data_len = get_data_cnt(pbuf);
        if(data_len < RAW_FRAME_SIZE * 2){
            cout << "Buf is not ready. size : " << data_len << endl;
            //usleep(10000*100);
            continue;
        }
        //cout << "Buff size is good: " << data_len << endl;

        start();
        char *frame_ptr;
        if((frame_ptr = get_a_frame(pbuf, pbuf->raw_buf, RAW_FRAME_SIZE)) != NULL){
            //stop("frame");
            cout << "ID " << pbuf->id << " : get frame: " << frame_idx++ << endl;
            //convert_raw_to_rgb24(pbuf->raw_buf, pbuf->rgb24buf);
            convert_raw_to_rgb24(frame_ptr, pbuf->rgb24buf);
            //stop("convert");
            if(pDisplay){
            	display_image(pDisplay, pbuf->rgb24buf, RGB24_BUFFER_SIZE);
             //   stop("display");
            }

            //printf("timediff %d  %d\n", tv_end.tv_sec - tv_start.tv_sec, tv_end.tv_usec - tv_start.tv_usec);

            //char tmps[60];
            //sprintf(tmps, "raw%d.raw", frame_idx);
            //save_buf_to_file(RAW_BUF, RAW_FRAME_SIZE, tmps);
            //sprintf(tmps, "./data/raw_%d_%d.ppm", pbuf->id, frame_idx);
            //save_buf_as_ppm(pbuf->rgb24buf, 1920 * 1920 * 3, tmps);
            //printf("convert done\n");
            //draw
        }
        else{
            printf("no frame\n");
            stop("no frame");
        }
        //cout << "continue.." << endl;
    }
    cout << "Processing buf is done on " << pbuf->sd << "cleaning up and closing....." << endl;
    cleanup_display(pDisplay);
    release_buffer(pbuf);
    //read buf and clean read parts
    //read_a_frame();
    //covert_to_jpg();
    //display_image();
    return NULL;
}
