#include <stdio.h>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <string.h>

using namespace std;

int main(){
    char s[8];
    char b[8];
    memset(s,'a', sizeof(s));
    memset(b,'b', sizeof(b));
    string str;
  //  sprintf(s,"abcdefgh");
    str=s;
    cout<<s<<endl;
}