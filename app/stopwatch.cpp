#include <stdio.h>
#include <time.h>
#include <sys/time.h>

static struct timeval tv_start;
static struct timeval tv_end;

void start(){
    gettimeofday(&tv_start, NULL);
}

void stop(const char *msg){
    gettimeofday(&tv_end, NULL);
    if(msg!= NULL){
        printf("stopwatch(%s): %ld %ld\n", msg, tv_end.tv_sec - tv_start.tv_sec, 
             tv_end.tv_usec - tv_start.tv_usec);
    }
    else{
        printf("stopwatch: %ld %ld\n", tv_end.tv_sec - tv_start.tv_sec, 
             tv_end.tv_usec - tv_start.tv_usec);
    }
    gettimeofday(&tv_start, NULL);
}

