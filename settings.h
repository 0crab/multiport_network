#ifndef MULTIPORT_NETWORK_SETTINGS_H
#define MULTIPORT_NETWORK_SETTINGS_H

int port_num;

#define PORT_NUM port_num

#define THREAD_NUM port_num

#define PORT_BASE 8033

#define INIT_READ_BUF_SIZE 1000
#define INIT_RET_BUF_SIZE 1000
#define BATCH_BUF_SIZE 1000000

#define MAX_CONN 65535

#define ROUND_NUM 20

#define TEST_WORK_STEP_SIZE 28

#endif //MULTIPORT_NETWORK_SETTINGS_H
