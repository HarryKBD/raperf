#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <udt.h>
#include "cc.h"
#include "test_util.h"
#include "buf.h"


#define MAX_CLIENT_SIZE 6
using namespace std;

char *recv_buf[MAX_CLIENT_SIZE];
void * recvdata(void *);
void *process_buf(void *id);


//#define RAW_FRAME_SIZE 5000000
#define RAW_FRAME_SIZE 1024*1024
char RAW_BUF[RAW_FRAME_SIZE];

int main(int argc, char * argv[])
{
    if ((1 != argc) && ((2 != argc) || (0 == atoi(argv[1]))))
    {
        cout << "usage: appserver [server_port]" << endl;
        return 0;
    }

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
        pbuf = init_buffer();
        pbuf->sd = UDPSOCKET(recver);

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

void * recvdata(void *p)
{

    FrameBuf *pbuf = (FrameBuf *)p;
    UDTSOCKET recver = pbuf->sd;

    char* buf;
    int buf_size = 100000;
    buf = new char[buf_size];
    int rs;
    int saved_len = 0;
    cout << "Creating recving packet from PI thread" << endl;

    while (1){
        //int var_size = sizeof(int);
        //UDT::getsockopt(recver, 0, UDT_RCVDATA, &rcv_size, &var_size);
        if (UDT::ERROR == (rs = UDT::recv(recver, buf, buf_size, 0)))
        {
           cout << "recvdata:" << UDT::getlasterror().getErrorMessage() << endl;
           break;
        }
        saved_len = save_data(pbuf, buf, rs);
        if(saved_len != rs){
            printf("Buffer might be full already..\n");
            usleep(5000);
            printf("Starting read again\n");
        }
    }
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

    cout << " client ID is : " << pbuf->sd << endl;

    while(1){
        data_len = get_data_cnt(pbuf);
        if(data_len < RAW_FRAME_SIZE * 2){
            cout << "Buf is not ready. size : " << data_len << endl;
            usleep(10000*100);
            continue;
        }
        cout << "Buff size is good: " << data_len << endl;

        if(get_a_frame(pbuf, RAW_BUF, RAW_FRAME_SIZE) != NULL){
            cout << "get frame: " << frame_idx++;
            //do something with this frame
        }
        cout << "continue.." << endl;
    }

    //read buf and clean read parts
    //read_a_frame();
    //covert_to_jpg();
    //display_image();
}
