#include "hash.h"
#include <string.h>
#include <stdio.h>

#define NUM 10000000

#define PORT_NUM 10

#define KEY_LEN 8
#define VALUE_LEN 8


uint64_t countlist[PORT_NUM];

int main(){
    hash_init();
    char key_buf[KEY_LEN + 1];
    char value_buf[VALUE_LEN + 1];

    uint8_t Pre_hash;

    for(char c = 'a'; c <= 'z'; c ++){

        for (int i = 0; i < NUM ; i++) {

            memset(key_buf, 0, sizeof(key_buf));

            sprintf(key_buf, "%d", i);

            memset(key_buf + strlen(key_buf), c, VALUE_LEN - strlen(key_buf));


            Pre_hash = (static_cast<uint8_t > (hash_func(key_buf, KEY_LEN))) % PORT_NUM;

            countlist[Pre_hash] ++;
        }
    }

    for(int i = 0;i < PORT_NUM; i++){
        printf("hash %d :%lu\n",i,countlist[i]);
    }
}