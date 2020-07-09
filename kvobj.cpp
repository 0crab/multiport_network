#include "kvobj.h"
#include "stdlib.h"
#include "assert.h"
#include "string.h"
#include "stdio.h"

kvobj * create_kvobj(uint16_t key_len, char * key, uint32_t value_len, char * value){
    kvobj * p = (kvobj *) malloc(2 + 4 + key_len + value_len);
    p->key_len = key_len;
    p->value_len = value_len;
    memcpy(KVOBJ_KEY(p),key,key_len);
    memcpy(KVOBJ_VALUE(p),value,value_len);
    return p;
}


bool kvobj_destory(kvobj * kv){
    assert(kv != NULL);
    free(kv);
    kv = NULL;
    return true;
};

void showkv(kvobj * kv){
    printf("key_len:%d\tvalue_len:%d\n",kv->key_len,kv->value_len);
    printf("%.*s:%.*s\n", kv->key_len, KVOBJ_KEY(kv),kv->value_len,KVOBJ_VALUE(kv));
}