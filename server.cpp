#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <assert.h>
#include <fcntl.h>


#include "server_define.h"


using namespace std;


void state_jump(conn_states &s_state, conn_states t_state) {
    s_state = t_state;
}


void init_portlist() {
    portList = (int *) (calloc(PORT_NUM, sizeof(int)));
    for (int i = 0; i < PORT_NUM; i++) portList[i] = PORT_BASE + i;
}

void init_conns() {
    int max_fds = MAX_CONN;
    if ((conns = (CONNECTION **) calloc(max_fds, sizeof(CONNECTION *))) == NULL) {
        fprintf(stderr, "Failed to allocate connection structures\n");
        /* This is unrecoverable so bail out early. */
        exit(1);
    }
}

void *worker(void *args) {
    int tid = *(int *) args;

//    printf("worker %d start \n", tid);

    THREAD_INFO *me = &threadInfoList[tid];

    event_base_loop(me->base, 0);

    event_base_free(me->base);
    return NULL;

}


void reset_cmd_handler(CONNECTION *c) {

    if (c->remaining_bytes > 0) {
        state_jump(c->state, conn_deal_with);
    } else {
        state_jump(c->state, conn_waiting);
    }
}

enum try_read_result try_read_network(CONNECTION *c) {
    enum try_read_result gotdata = READ_NO_DATA_RECEIVED;
    int res;
    int num_allocs = 0;
    assert(c != NULL);

    if (c->working_buf != c->read_buf) {
        if (c->remaining_bytes != 0) {
            memmove(c->read_buf, c->working_buf, c->remaining_bytes);
            //           printf("remaining %d bytes move ; ",c->remaining_bytes);
        } /* otherwise there's nothing to copy */
        c->working_buf = c->read_buf;
    }

    int avail = c->read_buf_size - c->remaining_bytes;

    res = read(c->sfd, c->read_buf + c->remaining_bytes, avail);

//    printf("receive %d\n",res);

    if (res > 0) {
        gotdata = READ_DATA_RECEIVED;
        c->recv_bytes = res;
        c->total_bytes = c->remaining_bytes + c->recv_bytes;
        c->remaining_bytes = c->total_bytes;
        c->worked_bytes = 0;
    }
    if (res == 0) {
        return READ_ERROR;
    }

    if (res == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return gotdata;
        }
    }
    return gotdata;
}

void conn_close(CONNECTION *c) {
    assert(c != NULL);

    event_del(&c->event);

    close(c->sfd);

    return;
}


void process_func(CONNECTION *c) {
    bool stop = false;

    int inst_count = 0;

    while (!stop) {

        switch (c->state) {

            case conn_read: {
                //        printf("[%d:%d] conn_read : ",c->thread_index, c->sfd);
                try_read_result res = try_read_network(c);
                switch (res) {
                    case READ_NO_DATA_RECEIVED:
                        state_jump(c->state, conn_waiting);
                        break;
                    case READ_DATA_RECEIVED:
                        state_jump(c->state, conn_deal_with);
                        break;
                    case READ_ERROR:
                        state_jump(c->state, conn_closing);
                        break;
                    case READ_MEMORY_ERROR: /* Failed to allocate more memory */
                        /* State already set by try_read_network */
                        perror("mem error");
                        exit(1);
                }
                break;
            }

            case conn_new_cmd : {
                //printf("conn_new_cmd :");
                if (++inst_count < ROUND_NUM) {
                    reset_cmd_handler(c);
                    //printf("reset and continue\n");
                } else {
                    stop = true;
                    //                printf("[%d:%d] stop and restart\n", c->thread_index,c->sfd);
                }
                break;
            }


            case conn_deal_with : {
                if (c->remaining_bytes < TEST_WORK_STEP_SIZE) {
                    state_jump(c->state, conn_waiting);
                    break;
                }

                //printf("conn_deal_with: ");
                //working with data 10 bytes every time
                char work_buf[TEST_WORK_STEP_SIZE];
                memcpy(work_buf, c->working_buf, TEST_WORK_STEP_SIZE);
                c->worked_bytes += TEST_WORK_STEP_SIZE;
                c->working_buf = c->read_buf + c->worked_bytes;
                c->remaining_bytes = c->total_bytes - c->worked_bytes;

                c->bytes_processed_in_this_connection += TEST_WORK_STEP_SIZE;

                state_jump(c->state, conn_new_cmd);
                break;
            }


            case conn_waiting : {
                //          printf("[%d:%d] conn_waiting\n",c->thread_index,c->sfd);
                state_jump(c->state, conn_read);
                stop = true;
                break;
            }

            case conn_closing : {
            //    printf("[%d:%d] conn_closing ,processed bytes: %lu \n", \
                        c->thread_index, c->sfd, c->bytes_processed_in_this_connection);
                conn_close(c);
                stop = true;
                break;
            }

        }
    }
}

void event_handler(const int fd, const short which, void *arg) {


    CONNECTION *c = (CONNECTION *) arg;

//    printf("[%d:%d] starting working,worked bytes:%d \n",c->thread_index, c->sfd, c->bytes_processed_in_this_connection);

    assert(c != NULL);

    /* sanity */
    if (fd != c->sfd) {
        fprintf(stderr, "Catastrophic: event fd doesn't match conn fd!\n");
        return;
    }

    process_func(c);

    /* wait for next event */
    return;
}

void conn_new(int sfd, struct event_base *base, int thread_index) {
    CONNECTION *c;

    assert(sfd >= 0 && sfd < MAX_CONN);

    c = conns[sfd];

    if (c == NULL) {
        if (!(c = (CONNECTION *) calloc(1, sizeof(CONNECTION)))) {
            fprintf(stderr, "Failed to allocate connection object\n");
            return;
        }
        c->read_buf_size = INIT_READ_BUF_SIZE;
        c->read_buf = (char *) malloc(c->read_buf_size * sizeof(char));

    }

    c->sfd = sfd;
    c->worked_bytes = 0;
    c->working_buf = c->read_buf;
    c->recv_bytes = 0;
    c->remaining_bytes = 0;
    c->bytes_processed_in_this_connection = 0;


    c->thread_index = thread_index;
    c->state = conn_read;  // default to read socket after accepting ;
    // worker will block if there comes no data in socket


    event_set(&c->event, c->sfd, EV_READ | EV_PERSIST, event_handler, (void *) c);
    event_base_set(base, &c->event);

    if (event_add(&c->event, 0) == -1) {
        perror("event_add");
        return;
    }

}

void thread_libevent_process(int fd, short which, void *arg) {

    THREAD_INFO *me = (THREAD_INFO *) arg;
    CONN_ITEM *citem = NULL;

    // printf("thread %d awaked \n",me->thread_index);
    char buf[1];

    if (read(fd, buf, 1) != 1) {
        fprintf(stderr, "Can't read from libevent pipe\n");
        return;
    }

    switch (buf[0]) {
        case 'c':
            pthread_mutex_lock(&me->conqlock);
            if (!me->connQueueList->empty()) {
                citem = me->connQueueList->front();
                me->connQueueList->pop();
            }
            pthread_mutex_unlock(&me->conqlock);
            if (citem == NULL) {
                break;
            }
            switch (citem->mode) {
                case queue_new_conn: {
                    conn_new(citem->sfd, me->base, me->thread_index);
                    break;
                }

                case queue_redispatch: {
                    //conn_worker_readd(citem);
                    break;
                }

            }
            break;
        default:
            fprintf(stderr, "pipe should send 'c'\n");
            return;

    }
    free(citem);

}


void setup_worker(int i) {
    THREAD_INFO *me = &threadInfoList[i];

    struct event_config *ev_config;
    ev_config = event_config_new();
    event_config_set_flag(ev_config, EVENT_BASE_FLAG_NOLOCK);
    me->base = event_base_new_with_config(ev_config);
    event_config_free(ev_config);


    if (!me->base) {
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

        threadInfoList[i].connQueueList = new queue<CONN_ITEM *>;


        setup_worker(i);
    }

    int ret;
    for (int i = 0; i < THREAD_NUM; i++) {
        if ((ret = pthread_create(&threadInfoList[i].thread_id,
                                  NULL,
                                  worker,
                                  (void *) &threadInfoList[i].thread_index)) != 0) {
            fprintf(stderr, "Can't create thread: %s\n",
                    strerror(ret));
            exit(1);
        }
        //pthread_exit(NULL);
    }

}


void conn_dispatch(evutil_socket_t listener, short event, void *args) {
    int port_index = *(int *) args;
    //printf("listerner :%d  port %d\n", listener, portList[port_index]);

    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr *) &ss, &slen);

    if (fd == -1) {
        perror("accept()");
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* these are transient, so don't log anything */
            return;
        } else if (errno == EMFILE) {
            fprintf(stderr, "Too many open connections\n");
            exit(1);
        } else {
            perror("accept()");
            return;
        }
    }

    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) < 0) {
        perror("setting O_NONBLOCK");
        close(fd);
        return ;
    }

    assert(fd > 2);


    CONN_ITEM *citem = (CONN_ITEM *) malloc(sizeof(CONN_ITEM));
    citem->sfd = fd;
    citem->thread = &threadInfoList[port_index];
    citem->mode = queue_new_conn;

    pthread_mutex_lock(&citem->thread->conqlock);
    citem->thread->connQueueList->push(citem);
    //printf("thread %d push citem, now cq size:%d\n",port_index,citem->thread->connQueueList->size());
    pthread_mutex_unlock(&citem->thread->conqlock);

    char buf[1];
    buf[0] = 'c';
    if (write(citem->thread->notify_send_fd, buf, 1) != 1) {
        perror("Writing to thread notify pipe");
    }
    // printf("awake thread %d\n", port_index);

}


int main(int argc, char **argv) {

    if (argc == 2) {
        port_num = atol(argv[1]);

        // batch_num = atol(argv[3]); //not used

    } else {
        printf("./multiport_network  <port_num> \n");
        return 0;
    }

    cout <<" port_num : " << port_num << endl;

    init_portlist();

    init_conns();

    init_workers();

//    int i;
//    const char **methods = event_get_supported_methods();
//    printf("Starting Libevent %s.  Available methods are:\n",
//           event_get_version());
//    for (i=0; methods[i] != NULL; ++i) {
//        printf("    %s\n", methods[i]);
//    }

    struct event_base *base;
    base = event_base_new();
    if (!base)
        return -1; /*XXXerr*/

    for (int i = 0; i < PORT_NUM; i++) {
        evutil_socket_t listener;
        struct event *listener_event;

        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = 0;
        sin.sin_port = htons(portList[i]);

        listener = socket(AF_INET, SOCK_STREAM, 0);
        evutil_make_socket_nonblocking(listener);


        if (bind(listener, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
            perror("bind");
            return -1;
        }

        if (listen(listener, 16) < 0) {
            perror("listen");
            return -1;
        }

        listener_event = event_new(base, listener,
                                   EV_READ | EV_PERSIST,
                                   conn_dispatch,
                                   (void *) &threadInfoList[i].thread_index);
        /*XXX check it */
        event_add(listener_event, NULL);
    }

    event_base_dispatch(base);

}


