#include "settings.h"

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event_struct.h>
#include <event2/event_compat.h>

#include <queue>

#ifndef MULTIPORT_NETWORK_DEFINE_H
#define MULTIPORT_NETWORK_DEFINE_H


enum conn_queue_item_modes {
    queue_new_conn,   /* brand new connection. */
    queue_redispatch, /* redispatching from side thread */
};

int * portList;

typedef struct CONNITEM CONN_ITEM;
typedef struct THREADINFO THREAD_INFO;

struct CONNITEM{
    int     sfd;    //socket handler
    enum    conn_queue_item_modes mode;
    char*   rbuf;    //read buf
    int     rbuf_size;    //read buf size
    int     rbytes;      //total bytes in buf
    char*   coffset;   //deal offset
    int     cbytes;     //bytes not deal with
    unsigned long recv_bytes;
    struct event event;
    THREAD_INFO * thread;
};

struct THREADINFO{
    int thread_index;
    //thread id
    pthread_t thread_id;        /* unique ID of this thread */
    //thread event base
    struct event_base *base;    /* libevent handle this thread uses */
    //Asynchronous event event
    struct event notify_event;  /* listen event for notify pipe */
    //pipe receive
    int notify_receive_fd;      /* receiving end of notify pipe */
    //pipe send
    int notify_send_fd;         /* sending end of notify pipe */

    std::queue<CONN_ITEM *>  *connQueueList; /* queue of new connections to handle */

    pthread_mutex_t conqlock;
  // struct queue_class * bufqueue;
};


//std::queue<CONN_ITEM>  * connQueueList;
//
THREAD_INFO * threadInfoList;



#endif //MULTIPORT_NETWORK_DEFINE_H
