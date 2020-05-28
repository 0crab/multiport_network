#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <assert.h>


#include "server_define.h"

using namespace std;


void init_portlist(){
    portList=(int *)(calloc(PORT_NUM, sizeof(int)));
    for(int i=0;i<PORT_NUM;i++) portList[i]=PORT_BASE+i;
}

void *worker(void * args){
    int tid=*(int *)args;

    printf("worker %d start \n",tid);

    THREAD_INFO *me =&threadInfoList[tid];

    event_base_loop(me->base, 0);

    event_base_free(me->base);
    return NULL;

}

void process_func(CONN_ITEM *citem){
    //printf("call process_func\n");
    //only read once to simulate sertain processing
    unsigned long bytes=0;
    int a=read(citem->sfd,citem->rbuf,citem->rbuf_size);
    bytes+=a;
    //printf("p%d:read %d\n",portList[citem->thread->thread_index],a);
    while(a!=0&&a!=-1){
        a=read(citem->sfd,citem->rbuf,citem->rbuf_size);
        bytes+=a;
        //printf("p%d:read %d\n",portList[citem->thread->thread_index],a);
    }
    printf("p%d:read %lu\n",portList[citem->thread->thread_index],bytes);

}

void event_handler(const int fd, const short which, void *arg) {
    CONN_ITEM *citem=(CONN_ITEM *)arg;

    assert(citem != NULL);

    /* sanity */
    if (fd != citem->sfd) {
        fprintf(stderr, "Catastrophic: event fd doesn't match conn fd!\n");
        return;
    }

    process_func(citem);

    /* wait for next event */
    return;
}

void conn_new(CONN_ITEM * citem,struct event_base *base){
    assert(citem->sfd >= 0);

    citem->rbuf_size=INIT_RBUF_SIZE;
    citem->rbuf=(char *)malloc(citem->rbuf_size*sizeof(char));
    citem->coffset=citem->rbuf;
    citem->rbytes=0;
    citem->cbytes=0;
    citem->recv_bytes=0;


    event_set(&citem->event, citem->sfd, EV_READ , event_handler, (void *) citem);
    event_base_set(base, &citem->event);

    if (event_add(&citem->event, 0) == -1) {
        perror("event_add");
        return ;
    }
}

void thread_libevent_process(int fd, short which, void *arg){
    THREAD_INFO *me =(THREAD_INFO *) arg;
    CONN_ITEM *citem=NULL;
    char buf[1];

    if (read(fd, buf, 1) != 1) {
        fprintf(stderr, "Can't read from libevent pipe\n");
        return;
    }

    switch (buf[0]) {
        case 'c':
            pthread_mutex_lock(&me->conqlock);
            if(!me->connQueueList->empty()){
                citem= me->connQueueList->front();
                me->connQueueList->pop();
            }
            pthread_mutex_unlock(&me->conqlock);
            if (citem==NULL) {
                break;
            }
            switch (citem->mode) {
                case queue_new_conn:
                    conn_new(citem,me->base);
                    break;
                case queue_redispatch:
                    //conn_worker_readd(citem);
                    break;
            }
            break;
        default:
            fprintf(stderr, "pipe should send 'c'\n");
            return;

    }

}


void setup_worker(int i){
    THREAD_INFO * me=&threadInfoList[i];


    struct event_config *ev_config;
    ev_config = event_config_new();
    event_config_set_flag(ev_config, EVENT_BASE_FLAG_NOLOCK);
    me->base = event_base_new_with_config(ev_config);
    event_config_free(ev_config);


    if (! me->base) {
        fprintf(stderr, "Can't allocate event base\n");
        exit(1);
    }

    /* Listen for notifications from other threads */
    event_set(&me->notify_event, me->notify_receive_fd,
              EV_READ | EV_PERSIST, thread_libevent_process, me);
    event_base_set(me->base, &me->notify_event);

    if (event_add(&me->notify_event, 0) == -1) {
        fprintf(stderr, "Can't monitor libevent notify pipe\n");
        exit(1);
    }


}

void init_workers() {
    //connQueueList=(std::queue<CONN_ITEM>*)calloc(THREAD_NUM,sizeof(std::queue<CONN_ITEM>));
    threadInfoList = (THREAD_INFO *) calloc(THREAD_NUM, sizeof(THREAD_INFO));
    //pthread_t tids[THREAD_NUM];
    for (int i = 0; i < THREAD_NUM; i++) {
        int fds[2];
        if (pipe(fds)) {
            perror("Can't create notify pipe");
            exit(1);
        }
        threadInfoList[i].notify_receive_fd = fds[0];
        threadInfoList[i].notify_send_fd = fds[1];

        threadInfoList[i].thread_index = i;

        threadInfoList[i].connQueueList=new queue<CONN_ITEM *>;


        setup_worker(i);
    }

    int ret;
    for (int i = 0; i < THREAD_NUM; i++) {
        if ((ret = pthread_create(&threadInfoList[i].thread_id,
                                    NULL,
                                    worker,
                                    (void *)&threadInfoList[i].thread_index) )!= 0) {
            fprintf(stderr, "Can't create thread: %s\n",
                    strerror(ret));
            exit(1);
        }
        //pthread_exit(NULL);
    }

}



void conn_dispatch(evutil_socket_t listener, short event, void * args) {
    int port_index = *(int *)args;
    printf("listerner :%d  port %d\n", listener, portList[port_index]);

    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr *) &ss, &slen);
    if (fd < 0) {
        perror("accept");
    } else if (fd > FD_SETSIZE) {
        close(fd);
    } else {
        CONN_ITEM *citem = (CONN_ITEM *) malloc(sizeof(CONN_ITEM));
        citem->sfd = fd;
        citem->thread = &threadInfoList[port_index];
        citem->mode = queue_new_conn;

        pthread_mutex_lock(&citem->thread->conqlock);
        citem->thread->connQueueList->push(citem);
        printf("thread %d push citem, now cq size:%d\n",port_index,citem->thread->connQueueList->size());
        pthread_mutex_unlock(&citem->thread->conqlock);

        char buf[1];
        buf[0] = 'c';
        if (write(citem->thread->notify_send_fd, buf, 1) != 1) {
            perror("Writing to thread notify pipe");
        }
        printf("awake thread %d\n", port_index);
    }
}


int main() {


    init_portlist();

    init_workers();


    struct event_base *base;
    base = event_base_new();
    if (!base)
        return -1; /*XXXerr*/

    for(int i=0;i<PORT_NUM;i++){
        evutil_socket_t listener;
        struct event *listener_event;

        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = 0;
        sin.sin_port = htons(portList[i]);

        listener = socket(AF_INET, SOCK_STREAM, 0);
        evutil_make_socket_nonblocking(listener);


        if (bind(listener, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
            perror("bind");
            return -1;
        }

        if (listen(listener, 16)<0) {
            perror("listen");
            return -1;
        }

        listener_event = event_new(base,listener,
                                   EV_READ|EV_PERSIST,
                                   conn_dispatch,
                                   (void *)&threadInfoList[i].thread_index);
        /*XXX check it */
        event_add(listener_event, NULL);
    }

    event_base_dispatch(base);

}


