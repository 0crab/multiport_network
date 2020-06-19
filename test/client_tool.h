#ifndef CLIENT_TOOL_H
#define CLIENT_TOOL_H

#include <stdlib.h>
#include <string>

using namespace std;

static enum protocol_mode{
    ASCII,
    BINARY
};

protocol_mode send_mode=BINARY;

bool my_connection_init(string ip_addr,string port,protocol_mode=BINARY);

bool my_get();

bool my_getb();

bool my_set();

bool my_setb();


#endif