#include "assoc.h"

void assoc_upsert(std::string key, std::string value){
    hashTable.insert_or_assign(key, value);
}

std::string assoc_find(std::string key){
    return hashTable.find(key);
}