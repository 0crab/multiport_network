#ifndef CLIENT_TOOL_H
#define CLIENT_TOOL_H

#include <stdlib.h>
#include <string>
#include "ThreadPool.h"

using namespace std;

static enum protocol_mode{
    ASCII,
    BINARY
};

protocol_mode send_mode=BINARY;


typedef struct kv_obj{
    char * key  ;
    size_t  key_len;
    char *value ;
    size_t  value_len;
}KV;



class ClientWorker{

public :

    ClientWorker() {}

    ClientWorker(const string target_ip, int port_base, int port_num, int worker_num);

    void op_get(KV);

    void op_set(KV);
private :



    vector<int> fds;
    ThreadPool threadPool;

};


#endif