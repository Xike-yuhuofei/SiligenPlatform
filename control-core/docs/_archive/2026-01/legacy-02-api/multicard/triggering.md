# MultiCard 触发控制

## 概述

MultiCard提供精确的位置触发和比较输出功能，支持基于位置的精确控制和同步输出。

## 核心功能

### 位置比较触发
- **比较输出**: 到达指定位置时输出信号
- **窗口比较**: 在指定位置范围内输出
- **脉冲计数**: 基于编码器脉冲的精确触发

### 缓冲区操作
- **数据缓冲**: 预存触发数据提高实时性
- **延时控制**: 精确的输出延时控制

## 主要API

### MC_CmpPluse - 比较脉冲输出

```c
int MC_CmpPluse(short nCardNum, short axis, long count, short mode,
                short outputMask, short outputDir, double delayTime)
```

**参数说明：**
- `axis`: 轴号（0-7）
- `count`: 脉冲计数值
- `mode`: 比较模式（0=单次，1=连续）
- `outputMask`: 输出掩码（位组合）
- `outputDir`: 输出方向（0=反向，1=正向）
- `delayTime`: 延时时间（毫秒）

**示例：**
```cpp
// 0轴到达10000脉冲时输出OUT0
MC_CmpPluse(1, 0, 10000, 0, 0x01, 1, 0.0);
```

### MC_BufCmpData - 缓冲比较数据

```c
int MC_BufCmpData(short nCardNum, short axis, long pos,
                  short outputMask, short activeLevel)
```

**示例：**
```cpp
// 在指定位置缓冲触发数据
MC_BufCmpData(1, 0, 5000, 0x01, 1);  // 5000位置触发OUT0
MC_BufCmpData(1, 0, 10000, 0x02, 1); // 10000位置触发OUT1
MC_BufCmpData(1, 0, 15000, 0x01, 0); // 15000位置关闭OUT0
```

### MC_BufWaitIO - 等待IO信号

```c
int MC_BufWaitIO(short nCardNum, short nCrdNum, short diMask,
                 short diLevel, double timeOut)
```

**参数说明：**
- `nCrdNum`: 坐标系号
- `diMask`: 输入掩码
- `diLevel`: 等待电平（0=低电平，1=高电平）
- `timeOut`: 超时时间（毫秒，0=无限等待）

**示例：**
```cpp
// 等待DI0为高电平，最长等待5秒
MC_BufWaitIO(1, 1, 0x01, 1, 5000.0);
```

### MC_BufDelay - 缓冲延时

```c
int MC_BufDelay(short nCardNum, short nCrdNum, double time)
```

**示例：**
```cpp
// 插入100ms延时
MC_BufDelay(1, 1, 100.0);
```

### MC_BufIO - IO操作

```c
int MC_BufIO(short nCardNum, short nCrdNum, short doMask, short doValue)
```

**示例：**
```cpp
// 设置OUT0和OUT1为高电平
MC_BufIO(1, 1, 0x03, 0x03);
```

## 应用示例

### 点胶应用示例

```cpp
// 点胶轨迹：移动 -> 点胶 -> 移动 -> 停止点胶

// 1. 移动到起始位置
MC_LnXY(1, 0, 0, 100.0, 500.0, 0.0, 0, 1);

// 2. 移动到点胶位置，同时准备触发
MC_BufCmpData(1, 0, 5000, 0x01, 1); // 5000位置开始点胶
MC_LnXY(1, 10000, 0, 50.0, 500.0, 0.0, 0, 2);

// 3. 继续移动，在10000位置停止点胶
MC_BufCmpData(1, 0, 10000, 0x01, 0); // 10000位置停止点胶
MC_LnXY(1, 20000, 0, 50.0, 500.0, 0.0, 0, 3);
```

### 多轴同步触发

```cpp
// X轴到达特定位置时触发Y轴动作
MC_CmpPluse(1, 0, 10000, 0, 0x01, 1, 0.0); // X轴10000时输出信号

// Y轴等待信号后继续
MC_BufWaitIO(1, 1, 0x01, 1, 0.0); // 等待信号
MC_LnXY(1, 0, 10000, 50.0, 500.0, 0.0, 0, 4);
```

### 精确定时输出

```cpp
// 创建精确的时序输出
MC_BufIO(1, 1, 0x01, 0x01);     // 输出OUT0
MC_BufDelay(1, 1, 100.0);        // 延时100ms
MC_BufIO(1, 1, 0x01, 0x00);     // 关闭OUT0
MC_BufDelay(1, 1, 50.0);         // 延时50ms
MC_BufIO(1, 1, 0x02, 0x01);     // 输出OUT1
MC_BufDelay(1, 1, 100.0);        // 延时100ms
MC_BufIO(1, 1, 0x02, 0x00);     // 关闭OUT1
```

## IO配置

### 数字输入输出

```c
// 设置数字输出
MC_SetExtDoBit(1, 0, 1);  // 设置OUT0为高电平

// 读取数字输入
short diValue = MC_GetExtDiValue(1);  // 读取所有DI状态

// 读取原始IO
short diRaw = MC_GetDiRaw(1);
```

### 模拟输入

```c
// 读取ADC值
short adcValue = MC_GetAdc(1, 0);  // 读取ADC通道0
```

## 状态查询

### 比较状态

```c
// 查询比较缓冲区状态
MC_CmpBufSts(1, 0);  // 查询0轴比较状态
```

## 注意事项

1. **时序精度**: 触发延时精度为1ms
2. **缓冲区大小**: 比较数据缓冲区有限，注意管理
3. **优先级**: 硬件触发优先级高于软件触发
4. **电平匹配**: 注意输入输出电平匹配（3.3V/5V/24V）
5. **滤波设置**: 机械开关输入需要滤波处理

## 错误处理

常见错误代码：
- **-1**: 参数错误
- **-2**: 缓冲区满
- **-3**: 轴号无效
- **-4**: 功能未启用

## 性能优化

1. **批量操作**: 使用缓冲区批量处理触发指令
2. **合理规划**: 提前规划触发点，减少实时计算
3. **信号隔离**: 使用光耦隔离提高抗干扰能力
4. **屏蔽干扰**: 远离强电磁干扰源