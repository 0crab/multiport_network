#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>
//#include "client_tool.h"
#include "settings.h"
//#include "tracer.h"


struct timeval starttime;
unsigned long runtime;

unsigned long getRunTime(struct timeval begTime) {
    struct timeval endTime;
    gettimeofday(&endTime, NULL);
    unsigned long duration = (endTime.tv_sec - begTime.tv_sec) * 1000000 + endTime.tv_usec - begTime.tv_usec;
    return duration;
}

void send_data(int threadid,socklen_t fd){
    char send_buf[2000];
    for(int i = 0;i < 100;i ++) send_buf[i] = i;

    memset(send_buf,1, sizeof(send_buf));
    write(fd,send_buf, sizeof(send_buf));
    printf("thread %d send %d\n",threadid, sizeof(send_buf));

}


void *thread_func(void *threadid){
    long tid;
    tid = (long) threadid;
    //printf("AF_INET\n");
    //create unix socket
    socklen_t connect_fd;
    connect_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(connect_fd < 0) {
        perror("cannot create communication socket");
        return NULL;
    }

    struct sockaddr_in srv_addr;
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(server_port_base + tid);
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //connect server
    socklen_t ret;
    ret = connect(connect_fd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    if(ret == -1) {
        perror("cannot connect to the server");
        close(connect_fd);
        return NULL;
    }

    send_data(tid,connect_fd);



}

int main(void)
{
    pthread_t threads[NUM_THREADS];
    int rc;
    int i;
    for (i = 0; i < NUM_THREADS; i++) {
        printf("main() : creating thread %d\n ", i);
        rc = pthread_create(&threads[i], NULL, thread_func, (void *) i);
        if (rc) {
            printf("Error:unable to create thread, %d\n", rc);
            return 1;
        }
    }
    pthread_exit(NULL);

}

