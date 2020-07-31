//
// Created by czl on 6/29/20.
//

#ifndef MULTIPORT_NETWORK_HASH_H
#define MULTIPORT_NETWORK_HASH_H

#include "jhash.h"
//#include "mhash.h"

uint32_t (*hash_func )(const void *key, size_t length);

void hash_init(){
    hash_func = jenkins_hash;
   //hash_func = MurmurHash3_x86_32;
}


#endif //MULTIPORT_NETWORK_HASH_H
