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
#include "Connection.h"
#include "settings.h"
#include "tracer.h"
#include "hash.h"
#include <vector>




using namespace std;


enum instructs {
    GET,
    SET,
    GETB,
    SETB,
};


char *my_database;

typedef struct Send_info{
    int thread_index;
    int fd;
    uint64_t send_bytes;
} send_info;

send_info ** info_matrix;

uint64_t g_offset = 0;
uint64_t g_count = 0;
bool stop = false;
bool clean = false;
mutex g_mutex ;


void con_database();

unsigned char get_opcode(instructs inst);

void data_dispatch(int tid);

void show_send_info();


instructs inst;
int thread_num = 1;
//int batch_num = 1;

const string server_ip = "127.0.0.1";

long * timelist;



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

    timelist = (long *)calloc(thread_num, sizeof(long));

    info_matrix =(send_info **) calloc(thread_num , sizeof(send_info*));
    for(int i = 0; i < port_num; i++){
        info_matrix[i] = (send_info *) calloc(port_num, sizeof(send_info));
    }

    double kv_n = KV_NUM;
    double p_l = PACKAGE_LEN;
    double data_size = (kv_n * p_l ) / 1000000000 ;
    cout << "worker : " << thread_num << endl
         << "kv_num : " << KV_NUM << endl
         << "data size : " << data_size << "GB" << endl
         << "port base : " << PORT_BASE << endl
         << "port num : " << port_num <<endl;

    if (in_inst == "get") inst = GET;
    else if (in_inst == "getb") inst = GETB;
    else if (in_inst == "set") inst = SET;
    else if (in_inst == "setb") inst = SETB;
    else {
        perror("please input correct instruction");
        return -1;
    }

    hash_init();

    con_database();

    vector<thread> threads;

    for(int i = 0;i < thread_num; i ++){
        printf("creating thread %d\n",i);
        threads.push_back(thread(data_dispatch,i));
    }
    for(int i = 0; i < thread_num; i++){
        threads[i].join();
        printf("thread %d stoped \n",i);
    }

    show_send_info();

    long avg_runtime = 0;
    for(int i = 0; i < thread_num; i++){
        avg_runtime += timelist[i];
    }
    avg_runtime /= thread_num;
    cout << "average runtime : " << avg_runtime << endl;


}


void con_database() {

    my_database = (char *) calloc(KV_NUM, PACKAGE_LEN);
    if(my_database == NULL ){
        perror("calloc database error\n");
    }

    unsigned long offset = 0;

    uint8_t Magic = 0x80;
    uint8_t Opcode = get_opcode(inst);
    uint16_t Key_length = KEY_LEN;
    uint16_t Batch_num = 1;
    uint8_t Pre_hash = 0;
    uint8_t Retain = 0;
    uint32_t Total_body_length = KEY_LEN + VALUE_LEN;

    char package_buf[100];
    char key_buf[KEY_LEN + 1];
    char value_buf[VALUE_LEN + 1];

    for(char c = 'a'; c <= 'z'; c ++){
        for (int i = 0; i < NUM ; i++) {
            memset(package_buf, 0, sizeof(package_buf));
            memset(key_buf, 0, sizeof(key_buf));
            memset(value_buf, 0, sizeof(value_buf));

            *(uint8_t *) HEAD_MAGIC(package_buf) = Magic;
            *(uint8_t *) HEAD_OPCODE(package_buf) = Opcode;
            *(uint16_t *) HEAD_KEY_LENGTH(package_buf) = htons(Key_length);
            *(uint16_t *) HEAD_BATCH_NUM(package_buf) = htons(Batch_num);
            *(uint8_t *) HEAD_RETAIN(package_buf) = Retain;
            *(uint32_t *) HEAD_BODY_LENGTH(package_buf) = htonl(Total_body_length);

            sprintf(key_buf, "%d", i);
            sprintf(value_buf, "%d", i);
            memset(key_buf + strlen(key_buf), c, VALUE_LEN - strlen(key_buf));
            memset(value_buf + strlen(value_buf), c + 1, VALUE_LEN - strlen(value_buf));
            Pre_hash = static_cast<uint8_t > ((hash_func(key_buf, KEY_LEN)) % port_num);


            memcpy(PACKAGE_KEY(package_buf), key_buf, KEY_LEN);
            memcpy(PACKAGE_VALUE(package_buf), value_buf, VALUE_LEN);
            *(uint8_t *) HEAD_PRE_HASH(package_buf) = Pre_hash;

            memcpy(my_database + offset, package_buf, PACKAGE_LEN );
            offset += PACKAGE_LEN;

        }
    }

}

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

void data_dispatch(int tid){
    Tracer t;

    vector <Connection> cons(port_num);

    for(int i = 0; i < port_num; i++){
        cons[i].init(i,server_ip);
        if( cons[i].get_fd() == -1) return ;
    }

   t.startTime();
//    char * work_buf;
//    bool end = false;
//    while(!stop){
//        g_mutex.lock();
//        if(stop) {   //other thread changed the stop signal before lock
//            g_mutex.unlock();
//            break;
//        }
//        work_buf = my_database + g_offset;
//        if(WORK_OP_NUM >= KV_NUM - g_count){
//            end = true;
//            stop = true;
//        }else{
//            g_offset += WORK_LEN;
//            g_count += WORK_OP_NUM;
//        }
//
//        g_mutex.unlock();
//        if(!end){
//            for(int i = 0; i < WORK_OP_NUM; i++){
//                uint8_t pre_hash =*(uint8_t *) HEAD_PRE_HASH(GET_PACKAGE(work_buf,i));
//                package_obj p;
//                p.package_ptr = GET_PACKAGE(work_buf,i);
//                p.package_len = PACKAGE_LEN;
//                cons[pre_hash].fetch_and_send(p);
//            }
//        }else{  //processing tail data
//            for(int i = 0; i < KV_NUM - g_count; i++){
//                uint8_t pre_hash =*(uint8_t *) HEAD_PRE_HASH(GET_PACKAGE(work_buf,i));
//                package_obj p;
//                p.package_ptr = GET_PACKAGE(work_buf,i);
//                p.package_len = PACKAGE_LEN;
//                cons[pre_hash].fetch_and_send(p);
//            }
//        }
//
//    }

#define GET_LOCAL_PACKAGE(i)  (my_database + local_offset + i * PACKAGE_LEN )

    uint64_t local_send_num = KV_NUM / thread_num;

    uint64_t local_offset = tid * local_send_num * PACKAGE_LEN;

    for(int i = 0 ; i< local_send_num ; i ++){
        uint8_t pre_hash =*(uint8_t *) HEAD_PRE_HASH(GET_LOCAL_PACKAGE(i));
        package_obj p;
        p.package_ptr = GET_LOCAL_PACKAGE(i);
        p.package_len = PACKAGE_LEN;
        cons[pre_hash].fetch_and_send(p);
    }


    for(int i = 0; i < CONNECTION_NUM; i++){
        cons[i].clean();
    }

    for(int i = 0; i < CONNECTION_NUM; i++){
        send_info ts;
        ts.thread_index = tid;
        ts.fd = cons[i].get_fd();
        ts.send_bytes = cons[i].get_send_bytes();
        info_matrix[tid][i] = ts;
    }


    uint64_t g_totalbytes = 0;
    for(int i = 0; i < CONNECTION_NUM; i++){
        g_totalbytes += cons[i].get_send_bytes();
    }
    printf("[%d] total send bytes :%lu\n",tid,g_totalbytes);

    timelist[tid] += t.getRunTime();

}

void show_send_info(){
    for(int i = 0; i < thread_num; i++){
        printf("thread %d :\n",i);
        for(int j = 0; j < CONNECTION_NUM; j++){
            printf("[%d,%d] : %lu\n",i,info_matrix[i][j].fd,info_matrix[i][j].send_bytes);
        }
    }
}