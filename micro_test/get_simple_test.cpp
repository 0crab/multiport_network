#include <cstdio>
#include <string>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

using namespace std;

#define RANGE 100000
#define TEST_NUM 10

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 8033

#define KEY_LEN  8

#define HEAD_LEN 12

#define HEAD_MAGIC(buf)           (buf)
#define HEAD_OPCODE(buf)          (buf + 1)
#define HEAD_KEY_LENGTH(buf)      (buf + 2)
#define HEAD_BATCH_NUM(buf)       (buf + 4)
#define HEAD_PRE_HASH(buf)        (buf + 6)
#define HEAD_RETAIN(buf)          (buf + 7)
#define HEAD_BODY_LENGTH(buf)     (buf + 8)
#define PACKAGE_KEY(buf)             (buf + HEAD_LEN)
#define PACKAGE_VALUE(buf)           (buf + HEAD_LEN + KEY_LEN)

#define PACKAGE_LEN (HEAD_LEN + KEY_LEN)

void show_query(char * send_buf){
    printf("query:%s\n",send_buf + 12);
}

void show_responce(char * rec_buf){
    printf("recv:%s\n",rec_buf + 12);
}

void con_send_buf(char * package_buf, int query_num){


    *(uint8_t *) HEAD_MAGIC(package_buf) = 0x80;
    *(uint8_t *) HEAD_OPCODE(package_buf) = 0x01;
    *(uint16_t *) HEAD_KEY_LENGTH(package_buf) =htons(KEY_LEN) ;
    *(uint16_t *) HEAD_BATCH_NUM(package_buf) = htons(1);
    *(uint8_t *) HEAD_RETAIN(package_buf) = 0;
    *(uint8_t *) HEAD_PRE_HASH(package_buf) = 0;
    *(uint32_t *) HEAD_BODY_LENGTH(package_buf) = htonl(KEY_LEN);

    char key_buf[KEY_LEN + 1];
    sprintf(key_buf, "%d", query_num);
    memset(key_buf + strlen(key_buf), 'a', KEY_LEN - strlen(key_buf));
    memcpy(PACKAGE_KEY(package_buf), key_buf, KEY_LEN);
}


int main(){
    unsigned int connect_fd;
    static struct sockaddr_in srv_addr;
    //create  socket
    connect_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(connect_fd < 0) {
        perror("cannot create communication socket");
        return -1;
    }

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(SERVER_PORT);
    srv_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);


    //connect server;
    if( connect(connect_fd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) == -1) {
        perror("cannot connect to the server");
        close(connect_fd);
        return -1;
    }

    char send_buf[100];
    char rec_buf[100];
    srand((unsigned)time(NULL));
    for(int i = 0; i < TEST_NUM; i ++){
        memset(send_buf, 0, sizeof(send_buf));
        memset(rec_buf, 0, sizeof(rec_buf));
        int query_num = rand() % RANGE;
        con_send_buf(send_buf,query_num);
        write(connect_fd, send_buf,PACKAGE_LEN);
        show_query(send_buf);
        read(connect_fd, rec_buf, 100);
        show_responce(rec_buf);
    }
}
