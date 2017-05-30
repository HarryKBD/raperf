#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <udt.h>
#include "cc.h"
#include "test_util.h"

using namespace std;

static UDTSOCKET socket_fd = -1;

void * monitor(void *);

int get_socket_fd(){
    return socket_fd;
}

int prepare_connection(char *ip, char *port)
{
    // Automatically start up and clean up UDT module.
    UDT::startup();

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
    //UDT::setsockopt(client, 0, UDT_SNDBUF, new int(10000000), sizeof(int));
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

    if (0 != getaddrinfo(ip, port, &hints, &peer))
    {
        cout << "incorrect server/peer address. " << ip << ":" << port << endl;
        return 0;
    }

    // connect to the server, implict bind
    if (UDT::ERROR == UDT::connect(client, peer->ai_addr, peer->ai_addrlen))
    {
        cout << "connect: " << ip << ":" << port << " " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }

    freeaddrinfo(peer);


   // pthread_create(new pthread_t, NULL, monitor, &client);

    socket_fd = client;
    cout << "client fd: " << client << endl;
    return client;
}

int send_to_server(char *buf, int len)
{
    int total_sent = 0;
    int sent = 0;


    if(socket_fd == -1){
        cout << "Socket is not ready for transmission." << endl;
        return 0;
    }

    cout << "socket in send : " << socket_fd << endl;

    while(total_sent < len)
    {
        sent = 0;
        if (UDT::ERROR == (sent = UDT::send(socket_fd, buf + total_sent, len - total_sent, 0)))
        {
            cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
            break;
        }

        total_sent += sent;
    }

    return total_sent;
}


void clean_transmission(){
    cout << "sending file done." << endl;
    //pthread_join()
    UDT::close(socket_fd);
    UDP::cleanup();
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
