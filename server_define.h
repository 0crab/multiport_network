#ifndef MULTIPORT_NETWORK_DEFINE_H
#define MULTIPORT_NETWORK_DEFINE_H


#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event_struct.h>
#include <event2/event_compat.h>

#include <queue>
#include "settings.h"
#include "kvobj.h"


typedef struct {
        uint8_t magic;
        uint8_t opcode;
        uint16_t keylen;
        uint16_t batchnum;
        uint8_t prehash;
        uint8_t reserved;
        uint32_t totalbodylen;
} protocol_binary_request_header;

typedef enum {
    PROTOCOL_BINARY_CMD_GET = 0x01,
    PROTOCOL_BINARY_CMD_SET = 0x04
} protocol_binary_command;

enum conn_queue_item_modes {
    queue_new_conn,   /* brand new connection. */
    queue_redispatch, /* redispatching from side thread */ /*not used*/
};

enum conn_states {
    conn_parse_cmd,

    conn_count,


    conn_deal_with,

    conn_new_cmd,    /**< Prepare connection for next command */
    conn_waiting,    /**< waiting for a readable socket */
    conn_read,       /**< reading in a command line */
 //   conn_parse_cmd,  /**< try to parse a command from the input buffer */
    conn_write,      /**< writing out a simple response */
    conn_nread,      /**< reading in a fixed number of bytes */
    conn_swallow,    /**< swallowing unnecessary bytes w/o storing */
    conn_closing,    /**< closing this connection */
    conn_mwrite,     /**< writing out many items sequentially */
    conn_closed,     /**< connection is closed */
    conn_watch,      /**< held by the logger thread as a watcher */
    conn_max_state   /**< Max state value (used for assertion) */
};

enum work_states {
    parse_head,
    read_key_value,
    store_kvobj,
    query_key,
    write_bcak,
};




int * portList;


typedef struct CONNECTION CONNECTION;
typedef struct CONNITEM CONN_ITEM;
typedef struct THREADINFO THREAD_INFO;

struct CONNECTION{
    int sfd;
    int thread_index;

    int     read_buf_size;          //read buf size
    char*   read_buf;               //read buf
    int     recv_bytes;
    int     total_bytes;
    char*   working_buf;             //deal offset
    int     worked_bytes;
    int     remaining_bytes;      //bytes not deal with

    int     ret_buf_size;
    char*   ret_buf;
    int     ret_buf_offset;
    int     ret_bytes;

    unsigned long      bytes_processed_in_this_connection;


    struct event event;

    conn_states conn_state;
    work_states work_state;


    protocol_binary_request_header binary_header;

    short cmd;

    std::string key;
    std::string value;

    bool query_hit;

    THREAD_INFO * thread;

    kvobj * kv;
};

struct CONNITEM{
    int     sfd;    //socket handler
    enum    conn_queue_item_modes mode;
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

enum try_read_result {
    READ_DATA_RECEIVED,
    READ_NO_DATA_RECEIVED,
    READ_ERROR,            /* an error occurred (on the socket) (or client closed connection) */
    READ_MEMORY_ERROR      /* failed to allocate more memory */
};

//std::queue<CONN_ITEM>  * connQueueList;
//
THREAD_INFO * threadInfoList;

CONNECTION ** conns;




typedef enum {
    PROTOCOL_BINARY_REQ = 0x80,
    PROTOCOL_BINARY_RES = 0x81
} protocol_binary_magic;

#endif //MULTIPORT_NETWORK_DEFINE_H
