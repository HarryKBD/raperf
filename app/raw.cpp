#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <iostream>
#include <udt.h>
#include <sys/time.h>

#include "fluxpiclient.h"

using namespace std;


char FRAME_MAGIC[] = {0x33, 0x44, 0x55, 0x44, 0x44, 0x77, 0x77, 0x27};
int MAGIC_LEN  = sizeof(FRAME_MAGIC);
char temp_frame[8];


char buf[1024*1024];
int buf_len = 1024*1024;

int main(int argc, char * argv[])
{
    int len;
    int framecount = 1000;

    if(prepare_connection("192.168.10.101", "9000") == 0){
    //if(prepare_connection("127.0.0.1", "9000") == 0){
        cout << " Connection error" << endl;
        return 0;
    }
    
    while(framecount-- > 0){
        len = send_to_server((char *)FRAME_MAGIC, MAGIC_LEN);
        cout << "sent bytes: " << len << endl;
        len = send_to_server(buf, buf_len);
        cout << "sent bytes: " << len << endl;
        usleep(10000);
    }

    char ch = getchar();
    cout << "done" << endl;

    clean_transmission();
    return 1;
}
