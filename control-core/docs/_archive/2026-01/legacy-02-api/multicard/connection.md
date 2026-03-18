# MultiCard 连接管理

## 概述

MultiCard控制卡通过UDP/IP以太网与PC通信，支持多卡同时连接。**注意：系统仅支持UDP协议，不支持TCP协议**。本章介绍设备连接、断开和基础控制功能。

## 核心API

### MC_Open - 建立连接

建立PC与MultiCard控制卡的网络连接。

```c
int MC_Open(short nCardNum, const char* pcPCAddr, short nPCPort, const char* pmcAddr, short nmcPort)
```

**参数说明：**
- `nCardNum`: 控制卡编号 (1-16)
- `pcPCAddr`: PC端IP地址
- `nPCPort`: PC端端口号
- `pmcAddr`: 控制卡IP地址
- `nmcPort`: 控制卡端口号

**示例：**
```cpp
// 基本连接
int result = MC_Open(1, "192.168.0.200", 0, "192.168.0.1", 0);
if (result == 0) {
    // 连接成功
    MC_Reset(); // 重置控制卡
} else {
    // 连接失败，处理错误
}
```

### MC_Close - 断开连接

断开与指定控制卡的连接。

```c
int MC_Close(short nCardNum)
```

**示例：**
```cpp
MC_Close(1); // 关闭1号控制卡
```

### MC_Reset - 硬件复位

复位指定控制卡到初始状态。

```c
int MC_Reset(short nCardNum)
```

### MC_Stop - 紧急停止

立即停止所有轴的运动。

```c
int MC_Stop(short nCardNum)
```

### MC_GetCardMessage - 获取错误信息

获取最近一次操作的错误信息。

```c
int MC_GetCardMessage(short nCardNum, char* pcMsg, int nMsgLen)
```

**示例：**
```cpp
char errorMsg[256];
MC_GetCardMessage(1, errorMsg, sizeof(errorMsg));
printf("Error: %s\n", errorMsg);
```

## 配置示例

### 单卡配置

```cpp
// 配置1号控制卡
MC_Open(1, "192.168.0.200", 0, "192.168.0.1", 0);
```

### 多卡配置

```cpp
// 第一张卡
MC_Open(1, "192.168.0.200", 0, "192.168.0.1", 0);

// 第二张卡
MC_Open(2, "192.168.0.200", 0, "192.168.0.2", 0);

// 第三张卡
MC_Open(3, "192.168.0.200", 0, "192.168.0.3", 0);
```

### 调试配置

```cpp
// 调试网络配置
MC_Open(1, "10.129.41.200", 0, "10.129.41.112", 0);
```

## 网络要求

1. **同一局域网**: PC和控制卡需要在同一网段
2. **IP不冲突**: 每个设备需要唯一IP地址
3. **端口分配**: 使用端口0让系统自动分配可用端口
4. **防火墙**: 确保动态分配的端口范围未被阻止

## 错误处理

常见连接错误及解决方案：

| 错误码 | 含义 | 解决方案 |
|--------|------|----------|
| -1 | 网络不可达 | 检查网线连接和IP配置 |
| -2 | 连接超时 | 检查控制卡是否上电 |
| -3 | 端口被占用 | 更换端口号或关闭占用程序 |
| -4 | 认证失败 | 检查控制卡配置 |

## 最佳实践

1. **连接检查**: 连接后立即检查返回值
2. **资源释放**: 程序退出前必须调用MC_Close
3. **错误日志**: 记录连接错误信息便于排查
4. **超时设置**: 设置合理的连接超时时间
5. **重连机制**: 实现自动重连提高可靠性

## 调试日志

启用调试日志辅助排查问题：

```c
MC_StartDebugLog(1, "debug.log"); // 开始记录日志
// ... 执行操作
MC_StopDebugLog(1); // 停止记录
```

## 注意事项

- 关闭WiFi避免网络干扰
- 使用静态IP地址而非DHCP
- 定期检查网络连接状态
- 多卡场景注意端口分配避免冲突