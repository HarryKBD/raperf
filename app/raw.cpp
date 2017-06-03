#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <udt.h>
#include <sys/time.h>

#include "fluxpiclient.h"

using namespace std;


char buf[1024];
int buf_len = 1024;

int main(int argc, char * argv[])
{
    int len;

    //if(prepare_connection("192.168.10.100", "9000") == 0){
    if(prepare_connection("127.0.0.1", "9000") == 0){
        cout << " Connection error" << endl;
        return 0;
    }
    
    len = send_to_server(buf, buf_len);

    cout << "sent bytes: " << len << endl;

    clean_transmission();
    return 1;
}
