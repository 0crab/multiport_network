#include <stdio.h>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "kvobj.h"

using namespace std;


int main(){
    char buf[50] = "abcdefghijk";
    string a = string(buf,3);
    cout << a << endl;
}