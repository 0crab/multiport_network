#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "client_tool.h"

ClientWorker::ClientWorker(string target_ip, int port_base, int port_num, int worker_num){

    for(int i = 0;i < port_num;i ++){
        socklen_t connect_fd;
        connect_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(connect_fd < 0) {
            perror("cannot create communication socket");
            return;
        }

        struct sockaddr_in srv_addr;
        srv_addr.sin_family = AF_INET;
        srv_addr.sin_port = htons(port_base + i);
        srv_addr.sin_addr.s_addr = inet_addr(target_ip.c_str());

        //connect server
        socklen_t ret;
        ret = connect(connect_fd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
        if(ret == -1) {
            perror("cannot connect to the server");
            close(connect_fd);
            return ;
        }

        fds.push_back(connect_fd);
    }

    this->threadPool.launch(worker_num);
}