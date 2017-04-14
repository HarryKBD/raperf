#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <udt.h>
#include "cc.h"
#include "test_util.h"

using namespace std;

void * recvdata(void *);

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

        pthread_t rcvthread;
        pthread_create(&rcvthread, NULL, recvdata, new UDTSOCKET(recver));
        pthread_detach(rcvthread);
    }

    UDT::close(serv);

    return 0;
}

static int save_to_file = 0;

void * recvdata(void * usocket)
{
    UDTSOCKET recver = *(UDTSOCKET *)usocket;
    delete (UDTSOCKET *)usocket;

    // aquiring file name information from client
    char file[1024];
    int len;

    if (UDT::ERROR == UDT::recv(recver, (char*)&len, sizeof(int), 0))
    {
        cout << "recvdata: " << UDT::getlasterror().getErrorMessage() << endl;
        return NULL;
    }

    if (UDT::ERROR == UDT::recv(recver, file, len, 0))
    {
       cout << "recvdata: " << UDT::getlasterror().getErrorMessage() << endl;
       return NULL;
    }
    file[len] = '\0';
    cout << " file name len: " << len << " saving data to: " << file << endl;

    // get size information
    int64_t size;

    if (UDT::ERROR == UDT::recv(recver, (char*)&size, sizeof(int64_t), 0))
    {
       cout << "recvdata: " << UDT::getlasterror().getErrorMessage() << endl;
       return NULL;
    }
    cout << "recv size would be " << size << endl;
    // receive the file
    //
    if(save_to_file){
        fstream ofs(file, ios::out | ios::binary | ios::trunc);
        int64_t recvsize; 
        int64_t offset = 0;

        if (UDT::ERROR == (recvsize = UDT::recvfile(recver, ofs, offset, size)))
        {
           cout << "recvdata: " << UDT::getlasterror().getErrorMessage() << endl;
           return NULL;
        }

        ofs.close();
    }
    else {
        //skipping saving file. just receive
        char* buf;
        int buf_size = 100000;
        buf = new char[buf_size];
        int64_t rcv_size = 0;
        int rs;
        cout << "skipping saving file" << endl;

        while (rcv_size < size){
            //int var_size = sizeof(int);
            //UDT::getsockopt(recver, 0, UDT_RCVDATA, &rcv_size, &var_size);
            if (UDT::ERROR == (rs = UDT::recv(recver, buf, buf_size, 0)))
            {
               cout << "recvdata:" << UDT::getlasterror().getErrorMessage() << endl;
               break;
            }
            rcv_size += rs;
        }
        delete [] buf;
    }

    cout << "recvdata: file " << file << " receiving is completed." << endl;
    UDT::close(recver);
    return NULL;
}
