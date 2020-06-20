#include <stdio.h>
#include <sys/time.h>
#include <netinet/in.h>

struct timeval starttime;

unsigned long runtime;

unsigned long getRunTime(struct timeval begTime) {
    struct timeval endTime;
    gettimeofday(&endTime, NULL);
    unsigned long duration = (endTime.tv_sec - begTime.tv_sec) * 1000000 + endTime.tv_usec - begTime.tv_usec;
    return duration;
}

int main(){
    gettimeofday(&starttime,NULL);
    for(__uint32_t i = 0; i < 100000000; i++){
        __uint32_t tmp = htonl(i);
        __uint32_t tmp1 = ntohl(tmp);
    }
    runtime = getRunTime(starttime);
    printf("%lu\n",runtime);
}