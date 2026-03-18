# open_port

## 函数签名
int open_port(const char* port_name, int baudrate);

## 参数
port_name: 串口名 (例如 "COM1")  
baudrate: 波特率 (例如 115200)

## 返回值
成功返回句柄，失败返回 -1

## 示例
int fd = open_port("COM3", 115200);

## 注意事项
- 调用前应确保端口未被占用
- Linux 平台下区分大小写