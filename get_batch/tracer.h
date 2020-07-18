#ifndef __MY_TRACER__
#define __MY_TRACER__

#include <sys/time.h>
#include <stdio.h>



class Tracer {
    timeval begTime;

    timeval endTime;

    long duration;

public:
    void startTime() {
        gettimeofday(&begTime, nullptr);
    }

    long getRunTime() {
        gettimeofday(&endTime, nullptr);
        //cout << endTime.tv_sec << "<->" << endTime.tv_usec << "\t";
        duration = (endTime.tv_sec - begTime.tv_sec) * 1000000 + endTime.tv_usec - begTime.tv_usec;
        begTime = endTime;
        return duration;
    }

    long fetchTime() {
        gettimeofday(&endTime, nullptr);
        duration = (endTime.tv_sec - begTime.tv_sec) * 1000000 + endTime.tv_usec - begTime.tv_usec;
        return duration;
    }
};
/*

 long runtime[TRACER_NUM];
 long mycount = 0;

Tracer tracers[TRACER_NUM];

bool show_flush =true; 
bool show_unix_sock=true;
bool show_network=true;
bool socket_error_f=true;

 void show_runtime(){
    if(mycount<TEST_TIMES-1) mycount++;
    else 
        for(int i=0;i<TRACER_NUM;i++){
            printf("tracer %d runtime: %ld\n",i,runtime[i]);
        }
}
*/
#endif