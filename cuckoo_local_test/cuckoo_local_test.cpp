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
#include <string>
#include <list>
#include "../libcuckoo/cuckoohash_map.hh"


using namespace std;


static libcuckoo::cuckoohash_map<std::string, std::string> hashTable;

typedef struct KVOBJ{
    string key;
    string value;
    uint8_t prehash;
}kvobj;


enum instructs {
    GET,
    SET,
    GETB,
    SETB,
};


vector<vector<kvobj>> mydatabase;

typedef struct Send_info{
    int thread_index;
    int fd;
    uint64_t send_bytes;
} send_info;

send_info ** info_matrix;

uint64_t g_offset = 0;
uint64_t g_count = 0;
int myround = ROUND_SET;
bool stop = false;
bool clean = false;
mutex g_mutex ;


void con_database();

unsigned char get_opcode(instructs inst);

void data_dispatch(int tid);



void pre_insert();
void show_send_info();


instructs inst;
int thread_num = 1;
//int batch_num = 1;

const string server_ip = "127.0.0.1";

long * timelist;



int main(int argc, char **argv) {

    string in_inst;
    if (argc == 3) {
        thread_num = atol(argv[1]);
        in_inst = string(argv[2]);
        // batch_num = atol(argv[3]); //not used

    } else {
        printf("./micro_test <thread_num>   <instruct> \n");
        return 0;
    }

    timelist = (long *)calloc(thread_num, sizeof(long));



    double kv_n = KV_NUM;
    double p_l = PACKAGE_LEN;
    double data_size = (kv_n * p_l * ROUND_SET) / 1000000000 ;
    cout << "worker : " << thread_num << endl
         << "kv_num : " << KV_NUM << endl
         << "round : " << ROUND_SET<< endl;

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

    pre_insert();

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


void con_database() {

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



    for(int i = 0;i<thread_num;i++){
        vector<kvobj> a;
        mydatabase.push_back(a);
    }

    unsigned long offset = 0;



    char package_buf[100];
    char key_buf[KEY_LEN + 1];
    char value_buf[VALUE_LEN + 1];

    for(size_t i = 0; i < KV_NUM; i++){

        uint64_t n = array[i] / 26;
        uint8_t c = array[i] % 26;

        memset(package_buf, 0, sizeof(package_buf));
        memset(key_buf, 0, sizeof(key_buf));
        memset(value_buf, 0, sizeof(value_buf));


        sprintf(key_buf, "%d", n);
        sprintf(value_buf, "%d", n);
        memset(key_buf + strlen(key_buf), 'a'+c, VALUE_LEN - strlen(key_buf));
        memset(value_buf + strlen(value_buf), 'a'+c+1 , VALUE_LEN - strlen(value_buf));
        uint8_t Pre_hash = static_cast<uint8_t > ((hash_func(key_buf, KEY_LEN)) % thread_num);

        kvobj tmpkv;
        tmpkv.key = string(key_buf,KEY_LEN);
        tmpkv.value = string(value_buf,VALUE_LEN);
        tmpkv.prehash = Pre_hash;

        mydatabase[Pre_hash].push_back(tmpkv);

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


void pre_insert(){
    for(int i = 0 ;i< thread_num ;i++){
        auto it = mydatabase[i].begin();
        while(it != mydatabase[i].end()){
            kvobj kv = *it;
            hashTable.insert_or_assign(kv.key,kv.value);
            it ++;
        }
    }
}

void data_dispatch(int tid){
    Tracer t;

    t.startTime();

    uint64_t  total_len =0;
    for(int i = 0;i<myround;i++){
        auto it = mydatabase[tid].begin();
        while(it != mydatabase[tid].end()){
            kvobj kv = *it;
            if(inst == GET){
                string v = hashTable.find(kv.key);
                total_len += v.length();
            }else if(inst == SET){
                hashTable.insert_or_assign(kv.key,kv.value);
            }else{
                perror("error inst");
                exit(-1);
            }
            it ++;
        }
    }

    timelist[tid] += t.getRunTime();
    //if(inst == GET ) printf("find v len %lu\n",total_len);
}

void show_send_info(){
    for(int i = 0; i < thread_num; i++){
        printf("thread %d :\n",i);
        for(int j = 0; j < CONNECTION_NUM; j++){
            printf("[%d,%d] : %lu\n",i,info_matrix[i][j].fd,info_matrix[i][j].send_bytes);
        }
    }
}