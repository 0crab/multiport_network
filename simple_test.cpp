#include <stdio.h>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <string.h>

using namespace std;

int main(){
    uint32_t a = 0x12345678;
    uint16_t b = static_cast<uint16_t> (a);
    printf("%x\n",b % 4);
}