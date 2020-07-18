#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <string.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <assert.h>
#include <mutex>
#include "settings.h"
#include "tracer.h"
#include "hash.h"
#include "generator.h"
#include <vector>
#include <random>
#include <arpa/inet.h>

using namespace std;


enum instructs {
    GET,
    SET,
    GETB,
    SETB,
};

typedef struct{
    char * buf;
    uint64_t offset;
    uint64_t datalen;
    uint16_t num;
}BATCH_OBJ;

BATCH_OBJ * batchObjList;


typedef struct{
    uint8_t magic;
    uint8_t opcode;
    uint16_t key_length;
    uint16_t batch_num;
    uint8_t pre_hash;
    uint8_t retain;
    uint32_t total_body_length;
}REQ_HEAD;

instructs inst;


int thread_num = 0;

long * timelist;

vector<vector<BATCH_OBJ>> database;

mutex * mutexlist;

uint64_t *total_send_bytes;
uint64_t *total_recv_bytes;

string server_ip = "127.0.0.1";

unsigned char get_opcode(instructs inst){
    switch (inst){
        case GET :
            return 0x01;
        case SET :
            return 0x04;
        case GETB :
            return 0x03;
        case SETB :
            return 0x06;
        default:
            perror("invalid instruct");
            return 0;
    }
}

void con_database(){
    double skew = SKEW;
    uint64_t range = KEY_RANGE;
    uint64_t count = KV_NUM;
    uint64_t * array =( uint64_t * ) calloc(count, sizeof(uint64_t));
    if (skew < zipf_distribution<uint64_t>::epsilon) {
        std::default_random_engine engine(
                static_cast<uint64_t>(chrono::steady_clock::now().time_since_epoch().count()));
        std::uniform_int_distribution<size_t> dis(0, range + 0);
        for (size_t i = 0; i < count; i++) {
            array[i] = static_cast<uint64_t >(dis(engine));
        }
    } else {
        zipf_distribution<uint64_t> engine(range, skew);
        mt19937 mt;
        for (size_t i = 0; i < count; i++) {
            array[i] = engine(mt);
        }
    }

    batchObjList = (BATCH_OBJ *)calloc(port_num, sizeof(BATCH_OBJ));
    for(int i = 0; i < port_num; i++){
        batchObjList[i].num = 0;
        batchObjList[i].offset = 0;
        batchObjList[i].datalen = HEAD_LEN + BATCH_NUM * PACKAGE_LEN;
        batchObjList[i].buf = (char *)calloc(1,batchObjList[i].datalen);

        REQ_HEAD req;
        req.magic = 0x80;
        req.opcode = 0x03;
        req.key_length = htons(KEY_LEN);
        req.batch_num = htons(BATCH_NUM);
        req.pre_hash = i;
        req.retain = 0;
        req.total_body_length = htonl(KEY_LEN);

        *(REQ_HEAD*)(batchObjList[i].buf) = req;

        batchObjList[i].offset += HEAD_LEN;

    }

    for(int i = 0;i < port_num; i++){
        vector<BATCH_OBJ> a;
        database.push_back(a);
    }

    char key_buf[KEY_LEN + 1];
    char packege_buf[PACKAGE_LEN];
    for(uint64_t i =0; i < KV_NUM ; i++){
        memset(key_buf, 0 , sizeof(key_buf));

        uint64_t n = array[i] / 26;
        uint8_t c = array[i] % 26;

        sprintf(key_buf, "%d", n);
        memset(key_buf + strlen(key_buf), 'a'+c, VALUE_LEN - strlen(key_buf));
        uint8_t Pre_hash = static_cast<uint8_t > ((hash_func(key_buf, KEY_LEN)) % port_num);

        REQ_HEAD req;
        req.magic = 0x80;
        req.opcode = 0x01;
        req.key_length = htons(KEY_LEN);
        req.pre_hash = Pre_hash;
        req.retain = 0;
        req.total_body_length = htonl(KEY_LEN);

        *(REQ_HEAD *)packege_buf = req;
        memcpy(packege_buf + sizeof(req), key_buf, KEY_LEN);

        BATCH_OBJ * batchobj = &batchObjList[Pre_hash];
        if(batchobj-> num >= BATCH_NUM -1){
            ((REQ_HEAD *)packege_buf)->batch_num = htons(batchobj->num);

            memcpy(batchobj->buf + batchobj->offset , packege_buf, PACKAGE_LEN);
            batchobj->num ++;
            batchobj->offset +=PACKAGE_LEN;

            database[Pre_hash].push_back(*batchobj);

            batchobj->num = 0;
            batchobj->offset = 0;
            batchobj->datalen = HEAD_LEN + BATCH_NUM * PACKAGE_LEN;
            batchobj->buf = (char *)calloc(1,batchobj->datalen);

            REQ_HEAD req;
            req.magic = 0x80;
            req.opcode = 0x03;
            req.key_length = htons(KEY_LEN);
            req.batch_num = htons(BATCH_NUM);
            req.pre_hash = Pre_hash;
            req.retain = 0;
            req.total_body_length = htonl(KEY_LEN);

            *(REQ_HEAD*)(batchobj->buf) = req;

            batchobj->offset += HEAD_LEN;

        }else{
            ((REQ_HEAD *)packege_buf)->batch_num = htons(batchobj->num);

            memcpy(batchobj->buf + batchobj->offset , packege_buf, PACKAGE_LEN);
            batchobj->num ++;
            batchobj->offset +=PACKAGE_LEN;
        }

    }

    uint64_t total_discarded_num = 0;
    //Clean up redundant data
    for(int i = 0; i< port_num;i++){
        total_discarded_num += batchObjList[i].num ;
        free(batchObjList[i].buf);
    }


    uint64_t total_num;
    for(int i = 0; i< port_num;i++){
        total_num += database[i].size() * BATCH_NUM;
        printf("port %d kv num :%d\n",i,database[i].size() * BATCH_NUM);
    }
    printf("ready num : %lu, dicarded num %lu\n",total_num, total_discarded_num);

    free(array);
}


bool fetch_and_send(uint32_t fd,int i,int tid){
    BATCH_OBJ batchObj;
    mutexlist[i].lock();
    if(database[i].size() == 0){
        mutexlist[i].unlock();
        return false;
    }else{
        auto it = database[i].begin();
        batchObj = *it;
        database[i].erase(it);
        mutexlist[i].unlock();
    }

    int ret;
    ret = write(fd, batchObj.buf, batchObj.datalen);
    if(ret == batchObj.datalen){
        total_send_bytes[tid] += ret;
    }else{
        //not an error ;but we stop here
        perror("write error");
        exit(-1);
    }


    uint32_t expected_bytes = (BATCH_NUM * (PACKAGE_LEN + VALUE_LEN));
    uint32_t  recbytes = 0;
    char rec_buf[4000];
    while (recbytes < expected_bytes){
        uint32_t ret = read(fd, rec_buf, 4000);
        recbytes += ret;
    }

    total_recv_bytes[tid] += recbytes;

    return true;
}


void data_dispatch(int tid){
    uint32_t * fds = (uint32_t *) calloc(port_num, sizeof(int));

    for(int i = 0; i < port_num; i++){
        unsigned int connect_fd;
        static struct sockaddr_in srv_addr;
        //create  socket
        connect_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(connect_fd < 0) {
            perror("cannot create communication socket");
            return ;
        }

        srv_addr.sin_family = AF_INET;
        srv_addr.sin_port = htons(PORT_BASE + i);
        srv_addr.sin_addr.s_addr = inet_addr(server_ip.c_str());


        //connect server;
        if( connect(connect_fd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) == -1) {
            perror("cannot connect to the server");
            close(connect_fd);
            return ;
        }

        fds[i] = connect_fd;
    }

    Tracer t;
    t.startTime();
    int stop_count;
    while(stop_count != port_num){
        stop_count = 0;
        for(int i = 0;i< port_num;i++){
            if(!fetch_and_send(fds[i],i,tid)){
                stop_count ++;
            }
        }
    }
    timelist[tid] += t.getRunTime();
    printf("thread %d,sendbytes:%lu\trecvbytes:%lu\n",tid,total_send_bytes[tid],total_recv_bytes[tid]);

}




int main(int argc, char **argv) {

    string in_inst;
    if (argc == 4) {
        thread_num = atol(argv[1]);
        port_num = atol(argv[2]);
        in_inst = string(argv[3]);
        // batch_num = atol(argv[3]); //not used

    } else {
        printf("./micro_test <thread_num>  <port_num> <instruct> \n");
        return 0;
    }

    double kv_n = KV_NUM;
    double p_l = PACKAGE_LEN;
    double data_size = (kv_n * p_l * ROUND_SET) / 1000000000 ;
    cout << "worker : " << thread_num <<  "\tport num : " << port_num << endl
         << "kv_num : " << KV_NUM << endl
         << "data size : " << data_size << "GB" << endl
         << "port base : " << PORT_BASE << endl;

    if (in_inst == "get") inst = GET;
    else if (in_inst == "getb") inst = GETB;
    else if (in_inst == "set") inst = SET;
    else if (in_inst == "setb") inst = SETB;
    else {
        perror("please input correct instruction");
        return -1;
    }

    timelist = (long *) calloc(thread_num, sizeof(long));

    hash_init();

    mutexlist = new mutex[port_num];

    total_send_bytes = new uint64_t[thread_num];
    total_recv_bytes = new uint64_t[thread_num];


    con_database();

    vector<thread> threads;

    for(int i = 0;i < thread_num; i ++){
        //printf("creating thread %d\n",i);
        threads.push_back(thread(data_dispatch,i));
    }
    for(int i = 0; i < thread_num; i++){
        threads[i].join();
        //printf("thread %d stoped \n",i);
    }

//    show_send_info();

    long avg_runtime = 0;
    for(int i = 0; i < thread_num; i++){
        avg_runtime += timelist[i];
    }
    avg_runtime /= thread_num;
    cout << "\n ** average runtime : " << avg_runtime << endl;
}