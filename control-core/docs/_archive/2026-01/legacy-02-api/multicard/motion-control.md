# MultiCard 运动控制

## 概述

MultiCard提供多种运动控制模式，包括直线插补、圆弧插补、JOG运动等。支持多轴协调运动和精确的速度控制。

## 核心概念

### 坐标系
- **坐标系号**: 1-16，用于标识不同的坐标系统
- **脉冲单位**: 位置以脉冲数表示，需要根据机械传动比转换
- **合成速度**: 多轴运动的合成速度（矢量速度）

### 运动模式
1. **梯形速度模式**: 匀加速-匀速-匀减速
2. **S型速度模式**: 加速度平滑过渡，减少冲击
3. **JOG模式**: 连续运动，用于手动调试

## 直线插补

### MC_LnXY - XY直线插补

```c
int MC_LnXY(short nCrdNum, long x, long y, double synVel, double synAcc,
             double velEnd, short nFifoIndex, long segNum)
```

**参数说明：**
- `nCrdNum`: 坐标系号 (1-16)
- `x, y`: 目标位置（脉冲）
- `synVel`: 合成速度（脉冲/毫秒）
- `synAcc`: 合成加速度（脉冲/毫秒²）
- `velEnd`: 终点速度（0表示完全停止）
- `nFifoIndex`: FIFO索引（通常为0）
- `segNum`: 段号（用于轨迹跟踪）

**示例：**
```cpp
// 从当前位置移动到(10000, 20000)
MC_LnXY(1, 10000, 20000, 100.0, 500.0, 0.0, 0, 1);
```

### MC_LnXYZ - XYZ直线插补

```c
int MC_LnXYZ(short nCrdNum, long x, long y, long z, double synVel, double synAcc,
              double velEnd, short nFifoIndex, long segNum)
```

## 圆弧插补

### MC_ArcXYC - XY平面圆弧插补

```c
int MC_ArcXYC(short nCrdNum, long x, long y, long i, long j, short dir,
               double synVel, double synAcc, double velEnd, short nFifoIndex, long segNum)
```

**参数说明：**
- `x, y`: 终点坐标
- `i, j`: 圆心相对于起点的偏移
- `dir`: 方向（1=顺时针，2=逆时针）

**示例：**
```cpp
// 绘制半圆，半径10000
MC_ArcXYC(1, 20000, 0, 10000, 0, 1, 100.0, 500.0, 0.0, 0, 2);
```

## JOG运动

### MC_PrfJog - 设置JOG模式

```c
int MC_PrfJog(short nCardNum, short axis)
```

### MC_SetJogPrm - 设置JOG参数

```c
int MC_SetJogPrm(short nCardNum, short axis, double vel, double acc)
```

**示例：**
```cpp
// 设置0轴JOG参数
MC_PrfJog(1, 0);
MC_SetJogPrm(1, 0, 500.0, 5000.0); // 速度500, 加速度5000
MC_SetVel(1, 0, 300.0); // 设置JOG速度为300
```

## 位置和速度设置

### MC_SetPos - 设置目标位置

```c
int MC_SetPos(short nCardNum, short axis, long pos)
```

### MC_SetVel - 设置速度

```c
int MC_SetVel(short nCardNum, short axis, double vel)
```

### MC_Update - 更新位置

```c
int MC_Update(short nCardNum, short axis)
```

## 运动参数配置

### 梯形速度参数

```c
int MC_SetTrapPrm(short nCardNum, short axis, double acc, double dec)
```

- `acc`: 加速度（脉冲/毫秒²）
- `dec`: 减速度（脉冲/毫秒²）

## 完整运动示例

### 矩形轨迹

```cpp
// 定义矩形四个角点
long positions[][2] = {
    {0, 0},      // 起点
    {10000, 0},  // 右上角
    {10000, 10000}, // 右下角
    {0, 10000},  // 左下角
    {0, 0}       // 回到起点
};

// 执行矩形轨迹
for (int i = 1; i < 5; i++) {
    MC_LnXY(1, positions[i][0], positions[i][1],
            100.0, 500.0, 0.0, 0, i);

    // 等待运动完成
    while (true) {
        long space;
        MC_GetLookAheadSpace(1, &space);
        if (space == 0) break; // 缓冲区空，运动完成
        Sleep(10);
    }
}
```

### 圆形轨迹

```cpp
// 绘制完整圆形（4个90度圆弧）
long radius = 10000;
double vel = 100.0;
double acc = 500.0;

// 第一象限
MC_ArcXYC(1, radius, radius, radius, 0, 1, vel, acc, 0.0, 0, 1);

// 第二象限
MC_ArcXYC(1, 0, 2*radius, 0, radius, 1, vel, acc, 0.0, 0, 2);

// 第三象限
MC_ArcXYC(1, -radius, radius, -radius, 0, 1, vel, acc, 0.0, 0, 3);

// 第四象限
MC_ArcXYC(1, 0, 0, 0, -radius, 1, vel, acc, 0.0, 0, 4);
```

## 运动状态查询

### 查询当前位置

```c
long posX, posY;
MC_GetCrdPos(1, &posX, &posY);
```

### 查询当前速度

```c
double velX, velY;
MC_GetCrdVel(1, &velX, &velY);
```

### 查询规划空间

```c
long space;
MC_GetLookAheadSpace(1, &space);
```

## 注意事项

1. **单位换算**: 所有位置参数使用脉冲单位，需要根据机械结构进行换算
2. **速度限制**: 设置的速度和加速度必须在硬件允许范围内
3. **缓冲区管理**: 连续运动时注意FIFO缓冲区溢出
4. **坐标系统**: 确保坐标系已正确初始化（MC_InitLookAhead）
5. **轴状态**: 运动前确保轴已使能（MC_AxisOn）

## 错误处理

常见运动错误：
- **-1**: 参数错误（位置或速度超出范围）
- **-2**: 坐标系未初始化
- **-3**: 轴未使能
- **-4**: 缓冲区满

## 性能优化

1. **批量传输**: 使用MC_CrdData批量发送运动指令
2. **前瞻控制**: 合理设置前瞻缓冲区大小
3. **速度平滑**: 使用S型加减速减少机械冲击
4. **精度控制**: 根据应用选择合适的位置精度