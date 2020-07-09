#ifndef MULTIPORT_NETWORK_KVOBJ_H
#define MULTIPORT_NETWORK_KVOBJ_H


/*for protocol reasons,key_len should not greater than 65535 */

#include <cstdint>

#define KVOBJ_LEN (2 + 4 + key_len + value_len)

#define KVOBJ_KEY(kvobj_ptr)  (kvobj_ptr->kv)
#define KVOBJ_VALUE(kvobj_ptr)  ((char * )(kvobj_ptr->kv) + kvobj_ptr->key_len)

typedef struct KVOBJ{
    uint16_t key_len;
    uint32_t value_len;
    char kv[];
}kvobj;


kvobj * create_kvobj(uint16_t key_len, char * key, uint32_t value_len, char * value);

bool kvobj_destory(kvobj * kv);

void showkv(kvobj * kv);

#endif //MULTIPORT_NETWORK_KVOBJ_H
