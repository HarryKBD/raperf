#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <udt.h>
#include "cc.h"
#include "test_util.h"
#include <sys/time.h>

using namespace std;

//#define SEND_BUF_SIZE 50000 
#define SEND_BUF_SIZE 8000 
int64_t  SEND_FILE_SIZE = 1024*1024*1024;  //5GB

void * monitor(void *);

char send_buf[SEND_BUF_SIZE] = {0x03, };

int main(int argc, char * argv[])
{
    if ((4 != argc) || (0 == atoi(argv[2])))
    {
        cout << "usage: appclient server_ip server_port filename" << endl;
        return 0;
    }

    // Automatically start up and clean up UDT module.
    UDTUpDown _udt_;

    struct addrinfo hints, *local, *peer;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    //hints.ai_socktype = SOCK_DGRAM;

    if (0 != getaddrinfo(NULL, "9000", &hints, &local))
    {
        cout << "incorrect network address.\n" << endl;
        return 0;
    }

    UDTSOCKET client = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

    // UDT Options
    //UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
    //UDT::setsockopt(client, 0, UDT_MSS, new int(9000), sizeof(int));
    UDT::setsockopt(client, 0, UDT_SNDBUF, new int(10000000), sizeof(int));
    //UDT::setsockopt(client, 0, UDP_SNDBUF, new int(10000000), sizeof(int));
    //UDT::setsockopt(client, 0, UDT_MAXBW, new int64_t(12500000), sizeof(int));

    // Windows UDP issue
    // For better performance, modify HKLM\System\CurrentControlSet\Services\Afd\Parameters\FastSendDatagramThreshold

    // for rendezvous connection, enable the code below
    /*
    UDT::setsockopt(client, 0, UDT_RENDEZVOUS, new bool(true), sizeof(bool));
    if (UDT::ERROR == UDT::bind(client, local->ai_addr, local->ai_addrlen))
    {
       cout << "bind: " << UDT::getlasterror().getErrorMessage() << endl;
       return 0;
    }
    */

    freeaddrinfo(local);

    if (0 != getaddrinfo(argv[1], argv[2], &hints, &peer))
    {
        cout << "incorrect server/peer address. " << argv[1] << ":" << argv[2] << endl;
        return 0;
    }

    // connect to the server, implict bind
    if (UDT::ERROR == UDT::connect(client, peer->ai_addr, peer->ai_addrlen))
    {
        cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }

    freeaddrinfo(peer);


    // send name information of the requested file
    int len = strlen(argv[3]);

    if (UDT::ERROR == UDT::send(client, (char*)&len, sizeof(int), 0))
    {
        cout << "send: " << UDT::getlasterror().getErrorMessage() << endl;
        return -1;
    }

    if (UDT::ERROR == UDT::send(client, argv[3], len, 0))
    {
        cout << "send: " << UDT::getlasterror().getErrorMessage() << endl;
        return -1;
    }

    int64_t send_size = SEND_FILE_SIZE*20;

    // send file size information
    if (UDT::ERROR == UDT::send(client, (char*)&send_size, sizeof(int64_t), 0))
    {
       cout << "send: " << UDT::getlasterror().getErrorMessage() << endl;
       return 0;
    }

    cout << "sending file: " << argv[3] << "  size: " << send_size << endl;

    pthread_create(new pthread_t, NULL, monitor, &client);


    int64_t total_sent = 0;
    int ss;
    struct timeval tv_start, tv_end;

    gettimeofday(&tv_start, NULL);


    while(total_sent < send_size)
    {
        int ssize = 0;

        int frag = 0;
        while (ssize < SEND_BUF_SIZE)
        {
            if (UDT::ERROR == (ss = UDT::send(client, send_buf + ssize, SEND_BUF_SIZE - ssize, 0)))
            {
                cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
                break;
            }
            //cout << ss << endl;

            frag++;
            ssize += ss;
        }
        //cout << "frag " << frag << endl;
        if (ssize < SEND_BUF_SIZE)
            break;
        total_sent += ssize;
    }
    
    gettimeofday(&tv_end, NULL);


    time_t taken = tv_end.tv_sec - tv_start.tv_sec;

    cout << "sending file done. saved file name : " << argv[3] << " total sent: " << total_sent << "( " << total_sent/(double)taken/1000000.0 << " MB/s) expected: " << SEND_FILE_SIZE << endl;

    UDT::close(client);
    return 0;
}

void * monitor(void * s)
{
    UDTSOCKET u = *(UDTSOCKET *)s;

    UDT::TRACEINFO perf;

    cout << "SendRate(Mb/s)\tRTT(ms)\tCWnd\tPktSndPeriod(us)\tRecvACK\tRecvNAK" << endl;

    while (true)
    {
        sleep(1);

        if (UDT::ERROR == UDT::perfmon(u, &perf))
        {
            cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
            break;
        }

        cout << perf.mbpsSendRate << "\t\t"
             << perf.msRTT << "\t"
             << perf.pktCongestionWindow << "\t"
             << perf.usPktSndPeriod << "\t\t\t"
             << perf.pktRecvACK << "\t"
             << perf.pktRecvNAK << endl;
    }

    return NULL;
}
