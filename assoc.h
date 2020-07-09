#ifndef MULTIPORT_NETWORK_ASSOC_H
#define MULTIPORT_NETWORK_ASSOC_H

#include "libcuckoo/cuckoohash_map.hh"

static libcuckoo::cuckoohash_map<std::string, std::string> hashTable;

void assoc_upsert(std::string key, std::string value);

std::string assoc_find(std::string);

#endif //MULTIPORT_NETWORK_ASSOC_H
