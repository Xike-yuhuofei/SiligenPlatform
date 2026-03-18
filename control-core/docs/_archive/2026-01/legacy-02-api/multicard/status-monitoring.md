# MultiCard 状态监控

## 概述

MultiCard提供全面的状态监控功能，实时获取设备状态、运动参数、错误信息和诊断数据，确保系统可靠运行。

## 监控类别

### 系统状态监控
- 卡连接状态
- 轴使能状态
- 运动状态
- 错误状态

### 运动参数监控
- 实时位置
- 实时速度
- 规划空间
- 负载状态

### 硬件状态监控
- IO状态
- 编码器状态
- 温度状态
- 通信状态

## 主要API

### MC_GetAllSysStatusSX - 获取系统状态

```c
int MC_GetAllSysStatusSX(short nCardNum, short axis, unsigned long* pStatus);
```

**状态位说明：**

| 位 | 含义 | 说明 |
|----|------|------|
| bit0 | 就绪 | 卡就绪 |
| bit1 | 警告 | 存在警告 |
| bit2 | 错误 | 存在错误 |
| bit3 | 运动中 | 轴正在运动 |
| bit4 | 伺服使能 | 伺服已使能 |
| bit5 | 到位 | 轴已到位 |
| bit6 | 正限位 | 触发正限位 |
| bit7 | 负限位 | 触发负限位 |
| bit8 | 急停 | 急停状态 |
| bit9 | 回零中 | 正在回零 |
| bit10 | 回零完成 | 回零操作完成 |
| bit11 | 回零成功 | 回零成功 |
| bit12 | 跟随误差 | 跟随误差过大 |
| bit13 | 位置超差 | 位置超差 |
| bit14-31 | 保留 | 预留扩展 |

**示例：**
```cpp
unsigned long status;
MC_GetAllSysStatusSX(1, 0, &status);

// 检查关键状态
bool isMoving = status & 0x0008;
bool isEnabled = status & 0x0010;
bool hasError = status & 0x0004;
bool homeDone = status & 0x0400;
```

### MC_GetCardMessage - 获取错误信息

```c
int MC_GetCardMessage(short nCardNum, char* pcMsg, int nMsgLen);
```

**示例：**
```cpp
char errorMsg[256];
MC_GetCardMessage(1, errorMsg, sizeof(errorMsg));
printf("Error: %s\n", errorMsg);
```

### MC_GetCrdPos - 获取当前位置

```c
int MC_GetCrdPos(short nCardNum, short nCrdNum, long* pPosX, long* pPosY);
```

### MC_GetCrdVel - 获取当前速度

```c
int MC_GetCrdVel(short nCardNum, short nCrdNum, double* pVelX, double* pVelY);
```

### MC_GetLookAheadSpace - 获取规划空间

```c
int MC_GetLookAheadSpace(short nCardNum, short nCrdNum, long* pSpace);
```

## IO状态监控

### MC_GetExtDiValue - 读取数字输入

```c
short MC_GetExtDiValue(short nCardNum);
```

**返回值：** 每个位对应一个DI输入
- bit0: DI0状态
- bit1: DI1状态
- ...

### MC_GetDiRaw - 读取原始DI

```c
short MC_GetDiRaw(short nCardNum);
```

### MC_GetAdc - 读取模拟输入

```c
short MC_GetAdc(short nCardNum, short nChannel);
```

**参数：**
- `nChannel`: ADC通道号（0-15）

## 编码器状态

### MC_EncOn/MC_EncOff - 使能/禁用编码器

```c
int MC_EncOn(short nCardNum, short axis, int encValue);
int MC_EncOff(short nCardNum, short axis);
```

### MC_GetEncOnOff - 获取编码器状态

```c
int MC_GetEncOnOff(short nCardNum, short axis, int* pEncValue);
```

## 应用示例

### 实时状态监控

```cpp
// 状态监控结构
typedef struct {
    bool isMoving;
    bool isEnabled;
    bool hasError;
    bool posLimit;
    bool negLimit;
    bool homeDone;
    long posX, posY;
    double velX, velY;
} SystemStatus;

// 获取系统状态
void GetSystemStatus(SystemStatus* status) {
    unsigned long sysStatus;

    // 获取轴状态
    MC_GetAllSysStatusSX(1, 0, &sysStatus);

    // 解析状态位
    status->isMoving = sysStatus & 0x0008;
    status->isEnabled = sysStatus & 0x0010;
    status->hasError = sysStatus & 0x0004;
    status->posLimit = sysStatus & 0x0040;
    status->negLimit = sysStatus & 0x0080;
    status->homeDone = sysStatus & 0x0400;

    // 获取位置和速度
    MC_GetCrdPos(1, 1, &status->posX, &status->posY);
    MC_GetCrdVel(1, 1, &status->velX, &status->velY);
}

// 主监控循环
void StatusMonitor() {
    SystemStatus status;

    while (true) {
        GetSystemStatus(&status);

        // 显示关键信息
        printf("Position: (%ld, %ld)\n", status.posX, status.posY);
        printf("Velocity: (%.2f, %.2f)\n", status.velX, status.velY);
        printf("Status: %s %s %s\n",
               status.isMoving ? "Moving" : "Stopped",
               status.isEnabled ? "Enabled" : "Disabled",
               status.hasError ? "Error" : "OK");

        // 错误处理
        if (status.hasError) {
            char errorMsg[256];
            MC_GetCardMessage(1, errorMsg, sizeof(errorMsg));
            printf("Error Message: %s\n", errorMsg);

            // 错误恢复逻辑
            HandleError(errorMsg);
        }

        Sleep(100);  // 100ms监控周期
    }
}
```

### 运动完成检测

```cpp
// 等待运动完成
bool WaitForMotionComplete(int timeoutMs) {
    int startTime = GetTickCount();

    while (true) {
        long status;
        MC_GetCrdStatus(1, 1, &status);

        // 检查运动完成
        if (status & 0x0010) {
            return true;  // 运动完成
        }

        // 检查超时
        if (GetTickCount() - startTime > timeoutMs) {
            printf("Motion timeout\n");
            return false;
        }

        // 检查错误
        if (status & 0x0004) {
            printf("Motion error\n");
            return false;
        }

        Sleep(10);
    }
}

// 使用示例
if (WaitForMotionComplete(30000)) {
    printf("Motion completed successfully\n");
} else {
    printf("Motion failed or timeout\n");
}
```

### IO状态监控

```cpp
// IO状态监控
void IOMonitor() {
    while (true) {
        // 读取数字输入
        short diValue = MC_GetExtDiValue(1);

        // 检查特定输入
        if (diValue & 0x01) {  // DI0为高
            printf("DI0 Active\n");
            // 触发相应动作
        }

        if (diValue & 0x02) {  // DI1为高
            printf("DI1 Active\n");
        }

        // 读取模拟输入
        for (int i = 0; i < 4; i++) {
            short adcValue = MC_GetAdc(1, i);
            printf("ADC%d: %d\n", i, adcValue);
        }

        Sleep(50);
    }
}
```

### 错误诊断和恢复

```cpp
// 错误处理函数
void HandleError(const char* errorMsg) {
    printf("Handling Error: %s\n", errorMsg);

    // 根据错误类型处理
    if (strstr(errorMsg, "Position Error") != NULL) {
        // 位置错误处理
        MC_Stop(1);  // 立即停止

        // 检当前位置
        long posX, posY;
        MC_GetCrdPos(1, 1, &posX, &posY);
        printf("Error Position: (%ld, %ld)\n", posX, posY);

        // 清空缓冲区
        MC_CrdClear(1, 1);

    } else if (strstr(errorMsg, "Limit") != NULL) {
        // 限位错误处理
        printf("Limit switch triggered\n");

        // 脱离限位
        // ... 处理逻辑

    } else if (strstr(errorMsg, "Communication") != NULL) {
        // 通信错误处理
        printf("Communication error, attempting reconnection\n");

        // 重新连接
        MC_Close(1);
        Sleep(1000);
        if (MC_Open(1, "192.168.0.200", 60000, "192.168.0.1", 60000) == 0) {
            printf("Reconnected successfully\n");
        }
    }
}
```

### 性能监控

```cpp
// 性能监控结构
typedef struct {
    int commandCount;      // 命令计数
    int errorCount;        // 错误计数
    double avgCycleTime;   // 平均周期时间
    double maxCycleTime;   // 最大周期时间
    long totalDistance;    // 总运行距离
} PerformanceMetrics;

PerformanceMetrics perf = {0};

// 性能监控
void PerformanceMonitor() {
    static int lastTime = GetTickCount();
    static long lastPosX = 0, lastPosY = 0;

    int currentTime = GetTickCount();
    long posX, posY;
    double velX, velY;

    // 获取当前位置和速度
    MC_GetCrdPos(1, 1, &posX, &posY);
    MC_GetCrdVel(1, 1, &velX, &velY);

    // 计算周期时间
    int cycleTime = currentTime - lastTime;
    perf.avgCycleTime = (perf.avgCycleTime + cycleTime) / 2.0;
    perf.maxCycleTime = max(perf.maxCycleTime, cycleTime);

    // 计算运行距离
    long dx = posX - lastPosX;
    long dy = posY - lastPosY;
    perf.totalDistance += sqrt(dx*dx + dy*dy);

    // 输出性能数据
    printf("Cycle Time: %.2f ms, Max: %.2f ms\n",
           perf.avgCycleTime, perf.maxCycleTime);
    printf("Total Distance: %ld\n", perf.totalDistance);
    printf("Error Count: %d\n", perf.errorCount);

    lastTime = currentTime;
    lastPosX = posX;
    lastPosY = posY;
}
```

## 错误代码

### 常见错误代码

| 代码 | 含义 | 处理方法 |
|------|------|----------|
| 0x1001 | 参数错误 | 检查输入参数 |
| 0x1002 | 设备未就绪 | 等待设备初始化 |
| 0x1003 | 通信错误 | 检查网络连接 |
| 0x1004 | 缓冲区满 | 减少发送频率 |
| 0x1005 | 限位触发 | 检查机械位置 |
| 0x1006 | 跟随误差 | 检查负载和参数 |
| 0x1007 | 编码器错误 | 检查编码器连接 |

## 注意事项

1. **监控频率**: 合理设置监控频率，避免过度占用CPU
2. **错误优先**: 优先处理错误状态，确保系统安全
3. **数据一致性**: 读取多个参数时注意时间一致性
4. **资源管理**: 监控线程注意资源释放
5. **日志记录**: 重要状态变化应记录日志

## 最佳实践

1. **分层监控**: 系统级、轴级、IO级分层监控
2. **异常处理**: 完善的异常处理和恢复机制
3. **性能优化**: 使用事件驱动替代轮询（如可能）
4. **状态机**: 使用状态机管理复杂状态变化
5. **预防性维护**: 基于状态数据进行预防性维护