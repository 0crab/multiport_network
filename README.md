# MULTIPORT_NETWORK

这是一个多端口key-value缓存服务器模型。



## 安装

确保本地已安装 cmake 3.10以上版本和 libevent 2.0。



## 使用说明

**编译工程**：

```
$mkdir build
$cd build
$cmake  -DCMAKE_BUILD_TYPE=Release ..
$make
```

**运行服务器** ：

```
$./multiport_network 4  
```

参数设置为：`multiport_network <port_num>`

其他设置可以在`settings.h `中修改设置，修改之后需要重新编译生效，相关定义如下：

```
PORT_BASE           端口号基数
INIT_READ_BUF_SIZE  接收缓冲区大小
INIT_RET_BUF_SIZE   发送缓冲区大小（非batch）
BATCH_BUF_SIZE      单个发送缓冲区大小（batch）
ROUND_NUM           连接每次最多处理的任务数量
```



**运行SET测试：**

```
$./micro_test/micro_test 4 4 set
```

参数设置为： `micro_test <thread_num> <port_num> <instruction>`

其他参数可以在`micro_test/settings.h `中修改设置，修改之后需要重新编译生效，相关定义如下：

```
KEY_LEN		KEY的长度
VALUE_LEN 	VALUE的长度
KV_NUM		总数据大小
ROUND_SET	数据轮数
PORT_BASE	端口号基数
SEND_BATCH	一次发送的数据包个数
```

进行完SET之后，可以运行`./micro_test/get_simple_test`来从服务器获取一些数据来确认是否已经成功SET

相关参数在其源文件中定义，两个重要参数为：

```
RANGE		查询的数据范围，建议设置成与SET测试的KV_NUM同样的值
TEST_NUM	查询的数据个数，建议10-100
```



**运行GET测试：**

```
$./get_batch/get_batch_test 4 4 getb
```

参数设置为： `get_batch_test <thread_num> <port_num> <instruction>`

其他参数可以在`get_batch/settings.h `中修改设置，修改之后需要重新编译生效，相关定义如下：

```
KEY_LEN		KEY的长度
KV_NUM		总数据大小
ROUND_SET	数据轮数（目前没用）
PORT_BASE	端口号基数
BATCH_NUM	BATCH数据包个数
```

