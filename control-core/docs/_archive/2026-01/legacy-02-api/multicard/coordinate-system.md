# MultiCard 坐标系统

## 概述

MultiCard支持多达16个独立坐标系，每个坐标系可以进行复杂的运动控制，包括前瞻控制、速度规划和多轴协调运动。

## 核心概念

### 坐标系管理
- **坐标系号**: 1-16，每个坐标系独立管理
- **前瞻控制**: 预处理运动指令，实现平滑过渡
- **缓冲机制**: 高效的运动数据管理

### 运动控制
- **插补模式**: 支持2-3轴直线、圆弧插补
- **速度控制**: 合成速度控制，保证轨迹精度
- **空间管理**: 规划空间和剩余空间监控

## 主要API

### MC_InitLookAhead - 初始化前瞻控制

```c
int MC_InitLookAhead(short nCardNum, short nCrdNum, short fifoMode,
                    int nFifoSize, double smoothTime)
```

**参数说明：**
- `nCrdNum`: 坐标系号（1-16）
- `fifoMode`: FIFO模式（0=正常，1=平滑过渡）
- `nFifoSize`: FIFO大小（通常200-1000）
- `smoothTime`: 平滑过渡时间（毫秒）

**示例：**
```cpp
// 初始化1号坐标系
MC_InitLookAhead(1, 1, 1, 500, 20.0);
```

### MC_SetCrdPrm - 设置坐标系参数

```c
int MC_SetCrdPrm(short nCardNum, short nCrdNum, double maxAcc,
                double maxDec, double maxJerk, short stopDec)
```

**参数说明：**
- `maxAcc`: 最大加速度
- `maxDec`: 最大减速度
- `maxJerk`: 最大加加速度
- `stopDec`: 紧急停止减速度

**示例：**
```cpp
MC_SetCrdPrm(1, 1, 5000.0, 5000.0, 100000.0, 10000.0);
```

### MC_CrdStart - 启动坐标系

```c
int MC_CrdStart(short nCardNum, short nCrdNum)
```

### MC_CrdStop - 停止坐标系

```c
int MC_CrdStop(short nCardNum, short nCrdNum)
```

### MC_CrdClear - 清空坐标系缓冲

```c
int MC_CrdClear(short nCardNum, short nCrdNum)
```

### MC_CrdSpace - 设置规划空间

```c
int MC_CrdSpace(short nCardNum, short nCrdNum, long nSpace)
```

## 运动指令

### MC_CrdData - 发送运动数据

```c
int MC_CrdData(short nCardNum, short nCrdNum, long* pData)
```

**数据格式：**
```
pData[0]: 段类型（1=直线，2=圆弧）
pData[1]: 轴数量（2或3）
pData[2]: 合成速度
pData[3]: 合成加速度
pData[4]: 终点速度
pData[5]: X位置
pData[6]: Y位置
pData[7]: Z位置（3轴时）
pData[8]: 圆心X（圆弧时）
pData[9]: 圆心Y（圆弧时）
```

### 高级运动指令

```c
int MC_CrdMoveEX(short nCardNum, short nCrdNum, double vel, double acc,
                 double velEnd, long x1, long y1, long z1);

int MC_CrdMoveAccEX(short nCardNum, short nCrdNum, double vel, double acc,
                    double jerk, double velEnd, long x1, long y1, long z1);
```

## 状态查询

### MC_GetCrdStatus - 获取坐标系状态

```c
int MC_GetCrdStatus(short nCardNum, short nCrdNum, long* pStatus);
```

**状态位含义：**
- bit0: 坐标系空闲
- bit1: 坐标系运行中
- bit2: 缓冲区空
- bit3: 缓冲区满
- bit4: 运动完成

### MC_GetCrdPos - 获取当前位置

```c
int MC_GetCrdPos(short nCardNum, short nCrdNum, long* pPosX, long* pPosY);
```

### MC_GetCrdVel - 获取当前速度

```c
int MC_GetCrdVel(short nCardNum, short nCrdNum, double* pVelX, double* pVelY);
```

### MC_GetLookAheadSpace - 获取剩余空间

```c
int MC_GetLookAheadSpace(short nCardNum, short nCrdNum, long* pSpace);
```

## 应用示例

### 基础坐标系初始化

```cpp
// 初始化坐标系
short cardNum = 1;
short crdNum = 1;

// 设置前瞻参数
MC_InitLookAhead(cardNum, crdNum, 1, 500, 20.0);

// 设置运动参数
MC_SetCrdPrm(cardNum, crdNum, 5000.0, 5000.0, 100000.0, 10000.0);

// 设置规划空间
MC_CrdSpace(cardNum, crdNum, 1000000);

// 启动坐标系
MC_CrdStart(cardNum, crdNum);
```

### 批量运动数据

```cpp
// 准备运动数据数组
long moveData[1000][10];
int dataCount = 0;

// 生成矩形轨迹
long points[][2] = {
    {0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}, {0, 0}
};

for (int i = 0; i < 4; i++) {
    // 设置直线运动参数
    moveData[dataCount][0] = 1;      // 直线运动
    moveData[dataCount][1] = 2;      // 2轴
    moveData[dataCount][2] = 1000;   // 速度1000
    moveData[dataCount][3] = 5000;   // 加速度5000
    moveData[dataCount][4] = 0;      // 终点速度0
    moveData[dataCount][5] = points[i+1][0]; // 目标X
    moveData[dataCount][6] = points[i+1][1]; // 目标Y
    dataCount++;
}

// 批量发送数据
for (int i = 0; i < dataCount; i++) {
    MC_CrdData(1, 1, moveData[i]);
}
```

### 实时轨迹跟踪

```cpp
// 实时监控运动状态
while (true) {
    long status;
    MC_GetCrdStatus(1, 1, &status);

    // 检查运动状态
    if (status & 0x0002) {  // 运行中
        long posX, posY;
        double velX, velY;

        MC_GetCrdPos(1, 1, &posX, &posY);
        MC_GetCrdVel(1, 1, &velX, &velY);

        printf("Position: (%ld, %ld), Velocity: (%.2f, %.2f)\n",
               posX, posY, velX, velY);
    }

    // 检查是否完成
    if (status & 0x0010) {
        printf("Motion completed\n");
        break;
    }

    Sleep(10);
}
```

### 平滑过渡轨迹

```cpp
// 使用高级运动指令实现平滑过渡
double smoothVel = 800.0;
double smoothAcc = 4000.0;
double smoothJerk = 80000.0;

// 系列运动点
struct Point {
    long x, y;
} trajectory[] = {
    {0, 0}, {5000, 0}, {5000, 5000}, {10000, 5000}, {10000, 0}
};

// 执行平滑轨迹
for (int i = 1; i < 5; i++) {
    MC_CrdMoveAccEX(1, 1, smoothVel, smoothAcc, smoothJerk, 0.0,
                    trajectory[i].x, trajectory[i].y, 0);
}

// 等待完成
long space;
do {
    MC_GetLookAheadSpace(1, 1, &space);
    Sleep(10);
} while (space > 0);
```

## 性能优化

### 缓冲区管理

```cpp
// 优化缓冲区设置
short crdNum = 1;
int optimalFifoSize = 800;  // 根据运动复杂度调整
double smoothTime = 15.0;   // 平滑时间

MC_InitLookAhead(1, crdNum, 1, optimalFifoSize, smoothTime);
```

### 数据预加载

```cpp
// 批量预加载运动数据
for (int i = 0; i < 100; i++) {
    // 预计算轨迹点
    long x = i * 1000;
    long y = sin(i * 0.1) * 5000;

    MC_CrdMoveEX(1, 1, 500.0, 2000.0, 0.0, x, y, 0);
}
```

## 错误处理

### 常见错误代码

- **-1**: 坐标系号无效
- **-2**: 坐标系未初始化
- **-3**: 缓冲区满
- **-4**: 参数超出范围
- **-5**: 运动冲突

### 错误恢复

```cpp
// 错误恢复流程
if (MC_CrdStart(1, 1) != 0) {
    // 尝试停止并重新初始化
    MC_CrdStop(1, 1);
    Sleep(100);
    MC_CrdClear(1, 1);

    // 重新初始化
    MC_InitLookAhead(1, 1, 1, 500, 20.0);
    MC_SetCrdPrm(1, 1, 5000.0, 5000.0, 100000.0, 10000.0);
}
```

## 注意事项

1. **资源管理**: 每个坐标系占用系统资源，合理分配
2. **缓冲区溢出**: 避免过快发送运动指令
3. **状态同步**: 定期查询坐标系状态
4. **精度控制**: 合理设置速度和加速度参数
5. **安全停止**: 实现紧急停止机制

## 最佳实践

1. **预初始化**: 系统启动时初始化所有需要的坐标系
2. **批量传输**: 使用批量API提高效率
3. **状态监控**: 实时监控系统状态
4. **参数调优**: 根据应用调优运动参数
5. **容错设计**: 实现完善的错误处理机制