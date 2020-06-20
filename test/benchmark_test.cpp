#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <string.h>
#include <iostream>
#include <netinet/in.h>



#define HEAD_LEN 12
#define SUBHEAD_LEN  8

#define KEY_LEN 8
#define VALUE_LEN 8

#define NO_BATCH_PACKAGE_LEN    (HEAD_LEN + KEY_LEN + VALUE_LEN)
#define BATCH_PACKAGE_LEN(batch_num)   (HEAD_LEN + batch_num * (SUBHEAD_LEN + KEY_LEN + VALUE_LEN))
#define SUB_PACKAGE_LEN     (SUBHEAD_LEN + KEY_LEN + VALUE_LEN)

//Make sure that kv_num_per_thread is an integer multiple of batch_num

#define TOTAL_HEAD_LEN(use_batch) ( use_batch ? \
                            ( kv_num_per_thread / batch_num ) * HEAD_LEN + batch_num * SUBHEAD_LEN : \
                            kv_num_per_thread * HEAD_LEN )

#define TOTAL_BODY_LEN ( kv_num_per_thread * ( KEY_LEN + VALUE_LEN ) )

#define HEAD_MAGIC(buf)           (buf)
#define HEAD_OPCODE(buf)          (buf + 1)
#define HEAD_KEY_LENGTH(buf)      (buf + 2)
#define HEAD_BATCH_NUM(buf)       (buf + 4)
#define HEAD_PRE_HASH(buf)        (buf + 6)
#define HEAD_RETAIN(buf)          (buf + 7)
#define HEAD_BODY_LENGTH(buf)     (buf + 8)
#define PACKAGE_KEY(buf)             (buf + HEAD_LEN)
#define PACKAGE_VALUE(buf)           (buf + HEAD_LEN + KEY_LEN)

#define SUBHEAD_KEY_LENGTH(buf,i)         (buf + HEAD_LEN + i * SUB_PACKAGE_LEN)
#define SUBHEAD_FINISH(buf,i)             (buf + HEAD_LEN + i * SUB_PACKAGE_LEN + 2)
#define SUBHEAD_RETAIN(buf,i)             (buf + HEAD_LEN + i * SUB_PACKAGE_LEN + 3)
#define SUBHEAD_VALUE_LENGTH(buf,i)       (buf + HEAD_LEN + i * SUB_PACKAGE_LEN + 4)
#define SUBPACKAGE_KEY(buf,i)             (buf + HEAD_LEN + i * SUB_PACKAGE_LEN + SUBHEAD_LEN)
#define SUBPACKAGE_VALUE(buf,i)           (buf + HEAD_LEN + i * SUB_PACKAGE_LEN + SUBHEAD_LEN + KEY_LEN)


using namespace std;

int base_port = 8033;


enum instructs {
    GET,
    SET,
    GETB,
    SETB,
};


char *my_database;

vector<pair<string, string>> source_data;


void con_database();

void con_source_data();

unsigned char get_opcode(instructs inst);

void send_data(int tid);


instructs inst;

int batch_num = 1;
int kv_num_per_thread = 0;
int thread_num = 1;


void task1(string msg)
{
    cout << "task1 says: " << msg;
}

int main(int argc, char **argv) {

    string in_inst;
    if (argc == 5) {
        thread_num = atol(argv[1]);
        in_inst = string(argv[2]);
        kv_num_per_thread = atol(argv[3]);
        batch_num = atol(argv[4]);

    } else {
        printf("./benchmark_test <thread_num> <instruct> <kv_num_per_thread> <batch_num> ");
        return 0;
    }

    if (in_inst == "get") inst = GET;
    else if (in_inst == "getb") inst = GETB;
    else if (in_inst == "set") inst = SET;
    else if (in_inst == "setb") inst = SETB;
    else {
        perror("please input correct instruction");
        return -1;
    }

    con_source_data();


    con_database();

    auto it = source_data.begin();
    while(it != source_data.end()){
        cout << it->first << " " << it->second << endl;
        it ++;
    }


}

void con_source_data() {
    char key_buf[KEY_LEN + 1];
    char value_buf[VALUE_LEN + 1];
    string key_str, value_str;
    for (int i = 0; i < kv_num_per_thread; i++) {
        memset(key_buf, 0, sizeof(key_buf));
        memset(value_buf, 0, sizeof(value_buf));
        sprintf(key_buf, "%d", i);
        sprintf(value_buf, "%d", i);
        memset(key_buf + strlen(key_buf), 'k', VALUE_LEN - strlen(key_buf));
        memset(value_buf + strlen(value_buf), 'v', VALUE_LEN - strlen(value_buf));
        key_buf[KEY_LEN] = '\0';
        value_buf[VALUE_LEN] = '\0';
        key_str = key_buf;
        value_str = value_buf;
        source_data.push_back(make_pair(key_str, value_str));
    }
}


void con_database() {
    bool use_batch = true;
    if (batch_num == 1) use_batch = false;

    my_database = (char *) calloc(1, TOTAL_HEAD_LEN(use_batch) + TOTAL_BODY_LEN);

    if (!use_batch) {
        unsigned long offset = 0;

        uint8_t Magic = 0x80;
        uint8_t Opcode = get_opcode(inst);
        uint16_t Key_length = KEY_LEN;
        uint16_t Batch_num = 1;
        uint8_t Pre_hash = 0;
        uint8_t Retain = 0;
        uint32_t Total_body_length = KEY_LEN + VALUE_LEN;

        char package_buf[100];
        for (int i = 0; i < kv_num_per_thread; i++) {
            memset(package_buf, 0, sizeof(package_buf));

            *(uint8_t *) HEAD_MAGIC(package_buf) = Magic;
            *(uint8_t *) HEAD_OPCODE(package_buf) = Opcode;
            *(uint16_t *) HEAD_KEY_LENGTH(package_buf) = htons(Key_length);
            *(uint16_t *) HEAD_BATCH_NUM(package_buf) = htons(Batch_num);
            *(uint8_t *) HEAD_PRE_HASH(package_buf) = Pre_hash;
            *(uint8_t *) HEAD_RETAIN(package_buf) = Retain;
            *(uint32_t *) HEAD_BODY_LENGTH(package_buf) = htonl(Total_body_length);

            const char *key = source_data[i].first.c_str();
            const char *value = source_data[i].second.c_str();
            memcpy(PACKAGE_KEY(package_buf), key, KEY_LEN);
            memcpy(PACKAGE_VALUE(package_buf), value, VALUE_LEN);

            memcpy(my_database + offset, package_buf, NO_BATCH_PACKAGE_LEN );
            offset += NO_BATCH_PACKAGE_LEN;
        }

    }else{
        unsigned long offset = 0;

        uint8_t Magic = 0x80;
        uint8_t Opcode = get_opcode(inst);
        uint16_t Key_length = 0;
        uint16_t Batch_num = batch_num;
        uint8_t Pre_hash = 0;
        uint8_t Retain = 0;
        uint32_t Total_body_length = batch_num * (SUBHEAD_LEN + KEY_LEN + VALUE_LEN);

        int package_buf_len = batch_num * (SUBHEAD_LEN + KEY_LEN + VALUE_LEN)+100;
        char * package_buf = (char *)malloc(package_buf_len);

        for(int i = 0; i < kv_num_per_thread/batch_num; i++){
            memset(package_buf,0, package_buf_len);

            *(uint8_t *)HEAD_MAGIC(package_buf) = Magic;
            *(uint8_t *)HEAD_OPCODE(package_buf) = Opcode;
            *(uint16_t *)HEAD_KEY_LENGTH(package_buf) = htons(Key_length);
            *(uint16_t *)HEAD_BATCH_NUM(package_buf) = htons(Batch_num);
            *(uint8_t *)HEAD_PRE_HASH(package_buf) = Pre_hash;
            *(uint8_t *)HEAD_RETAIN(package_buf) = Retain;
            *(uint32_t *)HEAD_BODY_LENGTH(package_buf) = htonl(Total_body_length);


            uint16_t Sub_key_length = KEY_LEN;
            uint8_t  Finish = 0;
            uint8_t  Sub_retain = 0;
            uint32_t Sub_value_length = VALUE_LEN;

            for(int j = 0; j < batch_num; j++){
                Finish = (j == batch_num - 1) ? 1 : 0;

                *(uint16_t *)SUBHEAD_KEY_LENGTH(package_buf,j) = htons(Sub_key_length);
                *(uint8_t *)SUBHEAD_FINISH(package_buf,j) = Finish;
                *(uint8_t *)SUBHEAD_RETAIN(package_buf,j) = Sub_retain;
                *(uint32_t *)SUBHEAD_VALUE_LENGTH(package_buf,j) = htonl(Sub_value_length);

                const char * key = source_data[i * batch_num + j].first.c_str();
                const char * value = source_data[i * batch_num + j].second.c_str();

                memcpy(SUBPACKAGE_KEY(package_buf,j), key, KEY_LEN);
                memcpy(SUBPACKAGE_VALUE(package_buf,j), value, VALUE_LEN);

            }

            memcpy(my_database + offset, package_buf, BATCH_PACKAGE_LEN(batch_num) );
            offset += BATCH_PACKAGE_LEN(batch_num);
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

void send_data(int tid){
    //cout << thread::id() << " working " << endl;
    cout << " working " << endl;
}

