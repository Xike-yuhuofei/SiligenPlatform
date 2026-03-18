# MultiCard 回零控制

## 概述

MultiCard提供完整的轴回零功能，支持多种回零模式和限位保护，确保系统的精确归位和安全运行。

## 回零模式

### 1. 单限位回零
- 使用一个限位开关确定原点
- 简单快速，适用于精度要求不高的场合

### 2. 双限位回零
- 使用正负两个限位开关
- 可在行程任意位置回零

### 3. 编码器零位回零
- 使用编码器的Z信号（Index信号）
- 精度最高，适用于高精度应用

### 4. 软件限位
- 纯软件定义的限位
- 适用于无硬件限位的场合

## 主要API

### MC_HomeStart - 开始回零

```c
int MC_HomeStart(short nCardNum, short axis, short mode, long offset,
                 double vel1, double vel2, short limitMask)
```

**参数说明：**
- `axis`: 轴号（0-7）
- `mode`: 回零模式
  - 0: 正限位回零
  - 1: 负限位回零
  - 2: 编码器零位回零
  - 3: 软限位回零
- `offset`: 原点偏移（脉冲）
- `vel1`: 高速段速度（脉冲/毫秒）
- `vel2`: 低速段速度（脉冲/毫秒）
- `limitMask`: 限位开关掩码

**示例：**
```cpp
// 0轴负限位回零，高速1000，低速100
MC_HomeStart(1, 0, 1, 0, 1000.0, 100.0, 0x01);
```

### MC_HomeStop - 停止回零

```c
int MC_HomeStop(short nCardNum, short axis)
```

### MC_HomeSetPrm - 设置回零参数

```c
int MC_HomeSetPrm(short nCardNum, short axis, short mode, long offset,
                  double vel1, double vel2, long escDist, short homeDI)
```

**参数说明：**
- `escDist`: 脱离限位的距离（脉冲）
- `homeDI`: 原点信号输入位

### MC_LmtsOn/MC_LmtsOff - 使能/禁用限位

```c
int MC_LmtsOn(short nCardNum, short axis, long posLmt, long negLmt)
int MC_LmtsOff(short nCardNum, short axis)
```

**示例：**
```cpp
// 设置0轴软件限位：-100000到100000
MC_LmtsOn(1, 0, 100000, -100000);
```

### MC_GetLmtsOnOff - 查询限位状态

```c
int MC_GetLmtsOnOff(short nCardNum, short axis, long* pPosLmt, long* pNegLmt)
```

## 回零流程

### 标准回零流程

```cpp
// 1. 配置回零参数
MC_HomeSetPrm(1, 0, 1,      // 负限位回零
              1000,         // 原点偏移1000脉冲
              2000.0,       // 高速2000脉冲/ms
              200.0,        // 低速200脉冲/ms
              5000,         // 脱离距离5000脉冲
              0x01);        // 使用DI0作为原点信号

// 2. 使能限位
MC_LmtsOn(1, 0, 200000, -200000);

// 3. 开始回零
MC_HomeStart(1, 0, 1, 1000, 2000.0, 200.0, 0x01);

// 4. 等待回零完成
while (true) {
    // 查询轴状态
    unsigned long status;
    MC_GetAllSysStatusSX(1, 0, &status);

    // 检查是否回零完成
    if (status & 0x0010) {  // 回零完成标志
        break;
    }

    Sleep(10);
}

// 5. 检查回零结果
if (status & 0x0020) {  // 回零成功
    printf("Home completed successfully\n");
} else {
    printf("Home failed\n");
}
```

## 应用示例

### 高精度回零

```cpp
// 编码器零位回零，最高精度
MC_HomeSetPrm(1, 0, 2,      // 模式2：编码器零位
              0,            // 无偏移
              500.0,        // 低速避免错过Z信号
              100.0,        // 更低速度精确定位
              1000,         // 脱离距离
              0x01);        // Z信号输入

MC_HomeStart(1, 0, 2, 0, 500.0, 100.0, 0x00);  // 无限位，仅Z信号
```

### 快速回零

```cpp
// 双速回零，提高效率
MC_HomeSetPrm(1, 0, 0,      // 正限位回零
              500,          // 原点偏移500
              5000.0,       // 高速5000
              500.0,        // 低速500
              8000,         // 脱离距离8000
              0x01);        // 限位输入

// 高速接近，低速定位
MC_HomeStart(1, 0, 0, 500, 5000.0, 500.0, 0x01);
```

### 多轴同步回零

```cpp
// 多轴同时回零
for (int i = 0; i < 3; i++) {
    MC_HomeSetPrm(1, i, 1, 0, 2000.0, 200.0, 5000, 0x01 << i);
    MC_HomeStart(1, i, 1, 0, 2000.0, 200.0, 0x01 << i);
}

// 等待所有轴回零完成
bool allHomed = false;
while (!allHomed) {
    allHomed = true;
    for (int i = 0; i < 3; i++) {
        unsigned long status;
        MC_GetAllSysStatusSX(1, i, &status);
        if (!(status & 0x0010)) {  // 未完成
            allHomed = false;
            break;
        }
    }
    Sleep(10);
}
```

## 安全设置

### 软限位保护

```cpp
// 设置软件限位作为最后防线
MC_LmtsOn(1, 0, 300000, -300000);  // X轴
MC_LmtsOn(1, 1, 200000, -200000);  // Y轴
MC_LmtsOn(1, 2, 100000, -100000);  // Z轴
```

### 回零失败处理

```cpp
// 设置超时保护
int homeTimeout = 30000;  // 30秒超时
int homeStartTime = GetTickCount();

while (true) {
    unsigned long status;
    MC_GetAllSysStatusSX(1, 0, &status);

    // 检查完成
    if (status & 0x0010) {
        break;
    }

    // 检查超时
    if (GetTickCount() - homeStartTime > homeTimeout) {
        MC_HomeStop(1, 0);
        printf("Home timeout!\n");
        // 错误处理...
        break;
    }

    Sleep(10);
}
```

## 状态检查

### 回零状态标志

```cpp
unsigned long status;
MC_GetAllSysStatusSX(1, 0, &status);

// 回零相关标志位
bool isHoming = status & 0x0008;    // 正在回零
bool homeDone = status & 0x0010;    // 回零完成
bool homeOK = status & 0x0020;      // 回零成功
bool homeError = status & 0x0040;   // 回零错误
bool limitHit = status & 0x0100;    // 触发限位
```

## 注意事项

1. **机械间隙**: 回零方向应考虑机械间隙影响
2. **信号质量**: 确保限位开关信号稳定可靠
3. **速度设置**: 高速用于快速接近，低速用于精确定位
4. **原点偏移**: 根据实际需求设置合适的原点偏移
5. **环境因素**: 温度变化可能影响重复精度

## 错误诊断

### 常见问题及解决方案

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 回零失败 | 限位开关故障 | 检查开关连接和信号 |
| 精度不足 | 速度过快 | 降低回零速度 |
| 振动大 | 脱离距离不够 | 增加脱离距离 |
| 重复性差 | 机械松动 | 紧固机械连接 |

## 维护建议

1. **定期检查**限位开关功能
2. **清洁保养**开关触点
3. **校准精度**定期验证回零精度
4. **记录数据**跟踪回零精度变化趋势