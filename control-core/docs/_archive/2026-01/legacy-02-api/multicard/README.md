# MultiCard API文档

## 概述

MultiCard是GAS公司的多轴运动控制卡，为Siligen工业点胶控制系统提供高精度的运动控制和位置触发功能。

## 系统要求

- Windows 7/8/10/11 (64位)
- Visual Studio 2017或更新版本
- C++17标准支持
- MultiCard驱动程序 v3.0+

## 快速开始

```cpp
#include "MultiCardCPP.h"

// 1. 打开设备连接
short cardNum = MC_Open(0);

// 2. 配置轴参数
MC_SetTrapPrm(cardNum, 0, 1000.0, 10000.0); // 设置梯形速度参数
MC_SetJogPrm(cardNum, 0, 500.0, 5000.0);    // 设置JOG速度参数

// 3. 使能轴
MC_AxisOn(cardNum, 0);

// 4. 执行运动
MC_LnXY(cardNum, 10000, 20000, 1.0); // XY直线插补

// 5. 关闭连接
MC_Close(cardNum);
```

## API分类

### 基础控制
- [设备连接](./connection.md) - MC_Open, MC_Close等
- [轴控制](./motion-control.md) - MC_LnXY, MC_ArcXYC等
- [参数设置](./motion-control.md#参数设置) - MC_SetTrapPrm, MC_SetJogPrm等

### 高级功能
- [坐标系统](./coordinate-system.md) - 坐标系管理和插补
- [位置触发](./triggering.md) - 比较输出和位置触发
- [回零控制](./homing.md) - 自动回零和原点设置

### 状态监控
- [状态查询](./status-monitoring.md) - 设备状态和错误查询
- [诊断功能](./status-monitoring.md#诊断) - 系统诊断信息

## 错误代码

常见错误代码及处理方法见[状态监控](./status-monitoring.md#错误代码)。

## 注意事项

1. **线程安全**: API调用不是线程安全的，需要在单线程环境中使用
2. **资源管理**: 使用MC_Open打开的卡必须用MC_Close关闭
3. **参数范围**: 速度、加速度参数必须在硬件允许范围内
4. **状态检查**: 运动指令执行前应检查轴状态

## 更多信息

- [完整API参考](./reference/)
- [示例代码](./examples.md)
- [常见问题](./faq.md)