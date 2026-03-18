# 2. JOG运动API

JOG运动提供连续的运动控制功能，适用于手动调试、点动操作和实时速度控制场景。

## 2.1 JOG运动基础API

### MC_JogOn

**函数原型**:
```c
int MC_JogOn(short nAxisNum);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置指定轴为JOG运动模式，启用连续运动控制功能。

**典型应用场景**:
- 手动调试设备
- 点动操作控制
- 实时速度调节
- 设备校准过程

**代码示例**:
```c
int result = 0;

// 设置轴1-3为JOG运动模式
printf("设置JOG运动模式...\n");

for (int axis = 1; axis <= 3; axis++) {
    result = MC_JogOn(axis);
    if (result == 0) {
        printf("轴%d已设置为JOG运动模式\n", axis);
    } else {
        printf("轴%d设置JOG模式失败，错误码: %d\n", axis, result);
    }
}

printf("JOG运动模式设置完成\n");

// 配置JOG运动参数
for (int axis = 1; axis <= 3; axis++) {
    MC_SetJogPrmSingle(axis, 5.0, 5.0, 0.0, 0);
    printf("轴%d JOG参数: 速度=5.0, 加速度=5.0\n", axis);
}
```

**预期输出**:
```
设置JOG运动模式...
轴1已设置为JOG运动模式
轴2已设置为JOG运动模式
轴3已设置为JOG运动模式
JOG运动模式设置完成
轴1 JOG参数: 速度=5.0, 加速度=5.0
轴2 JOG参数: 速度=5.0, 加速度=5.0
轴3 JOG参数: 速度=5.0, 加速度=5.0
```

**异常处理机制**:
- 检查轴号范围
- 确认轴未被其他运动模式占用
- 验证轴状态正常

### MC_JogOff

**函数原型**:
```c
int MC_JogOff(short nAxisNum);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 关闭指定轴的JOG运动模式，停止连续运动。

**典型应用场景**:
- 停止手动调试
- 切换到其他运动模式
- 安全停止操作

**代码示例**:
```c
int result = 0;

// 关闭轴1的JOG运动
result = MC_JogOff(1);
if (result == 0) {
    printf("轴1 JGO运动已停止\n");
} else {
    printf("轴1停止JOG运动失败，错误码: %d\n", result);
}

// 批量停止多个轴的JOG运动
printf("停止所有轴的JOG运动...\n");
for (int axis = 1; axis <= 3; axis++) {
    result = MC_JogOff(axis);
    if (result == 0) {
        printf("轴%d JGO运动已停止\n", axis);
    }
}

printf("JOG运动全部停止完成\n");
```

**预期输出**:
```
轴1 JGO运动已停止
停止所有轴的JOG运动...
轴1 JGO运动已停止
轴2 JGO运动已停止
轴3 JGO运动已停止
JOG运动全部停止完成
```

**异常处理机制**:
- 确认轴处于JOG模式
- 等待轴完全停止后再切换模式
- 检查轴状态变化

### MC_JogStop

**函数原型**:
```c
int MC_JogStop(long axisMask);
```

**参数说明**:
- `axisMask` [输入]: 停止JOG运动的轴掩码
  - 类型: long
  - 范围: 0-4294967295
  - 单位: 无

**位定义说明**:
- Bit0: 停止轴1的JOG运动
- Bit1: 停止轴2的JOG运动
- Bit2: 停止轴3的JOG运动
- ...

**返回值**: 参见API返回值说明

**功能描述**: 批量停止指定轴的JOG运动，支持多轴同时停止。

**典型应用场景**:
- 急停操作
- 批量停止调试
- 安全保护触发

**代码示例**:
```c
int result = 0;

// 批量停止JOG运动
printf("批量停止JOG运动...\n");

// 停止轴1、轴2、轴3的JOG运动
result = MC_JogStop(0x07); // 0b00000111
if (result == 0) {
    printf("轴1、轴2、轴3的JOG运动已停止\n");
    printf("掩码值: 0x%02lX\n", 0x07);
}

// 停止轴4的JOG运动
result = MC_JogStop(0x08);
if (result == 0) {
    printf("轴4的JOG运动已停止\n");
}

// 显示位掩码对应关系
printf("\n位掩码对应关系:\n");
for (int i = 0; i < 8; i++) {
    long mask = 1 << i;
    printf("Bit%d (0x%02lX): 停止轴%d的JOG运动\n", i, mask, i + 1);
}
```

**预期输出**:
```
批量停止JOG运动...
轴1、轴2、轴3的JOG运动已停止
掩码值: 0x07
轴4的JOG运动已停止

位掩码对应关系:
Bit0 (0x01): 停止轴1的JOG运动
Bit1 (0x02): 停止轴2的JOG运动
Bit2 (0x04): 停止轴3的JOG运动
Bit3 (0x08): 停止轴4的JOG运动
Bit4 (0x10): 停止轴5的JOG运动
Bit5 (0x20): 停止轴6的JOG运动
Bit6 (0x40): 停止轴7的JOG运动
Bit7 (0x80): 停止轴8的JOG运动
```

**异常处理机制**:
- 检查轴掩码有效性
- 确认轴处于运动状态
- 验证停止操作权限

## 2.2 JOG运动参数配置

### MC_SetJogPrm

**函数原型**:
```c
int MC_SetJogPrm(short nAxisNum, TJogPrm *pPrm);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `pPrm` [输入]: JOG模式运动参数结构体指针
  - 类型: TJogPrm*
  - 包含: 加速度、减速度、最大速度等参数

**结构体定义**:
```c
typedef struct JogPrm {
    double acc;        // 加速度，单位：脉冲/ms²
    double dec;        // 减速度，单位：脉冲/ms²
    double maxVel;     // 最大速度，单位：脉冲/ms
    double smoothTime; // 平滑时间，单位：ms
} TJogPrm;
```

**返回值**: 参见API返回值说明

**功能描述**: 设置JOG模式的运动参数，包括速度、加速度等关键参数。

**典型应用场景**:
- JOG运动参数优化
- 不同负载的JOG调试
- 精密点动控制

**代码示例**:
```c
int result = 0;
TJogPrm jogPrm;

// 配置JOG运动参数
printf("配置JOG运动参数...\n");

// 初始化参数结构体
memset(&jogPrm, 0, sizeof(jogPrm));

// 设置JOG运动参数（适用于精密调试）
jogPrm.acc = 0.2;        // 加速度：0.2脉冲/ms²（低速，精确）
jogPrm.dec = 0.2;        // 减速度：0.2脉冲/ms²
jogPrm.maxVel = 3.0;     // 最大速度：3.0脉冲/ms
jogPrm.smoothTime = 50;  // 平滑时间：50ms

// 为轴1设置JOG参数
result = MC_SetJogPrm(1, &jogPrm);
if (result == 0) {
    printf("轴1 JGO运动参数设置成功\n");
    printf("  最大速度: %.2f 脉冲/ms\n", jogPrm.maxVel);
    printf("  加速度: %.2f 脉冲/ms²\n", jogPrm.acc);
    printf("  减速度: %.2f 脉冲/ms²\n", jogPrm.dec);
    printf("  平滑时间: %d ms\n", jogPrm.smoothTime);
} else {
    printf("轴1参数设置失败，错误码: %d\n", result);
}

// 为轴2设置高速JOG参数
jogPrm.acc = 1.0;        // 高加速度
jogPrm.dec = 1.0;        // 高减速度
jogPrm.maxVel = 20.0;    // 高速度
jogPrm.smoothTime = 20;  // 短平滑时间
result = MC_SetJogPrm(2, &jogPrm);

// 为轴3设置标准JOG参数
jogPrm.acc = 0.5;        // 标准加速度
jogPrm.dec = 0.5;        // 标准减速度
jogPrm.maxVel = 10.0;    // 标准速度
jogPrm.smoothTime = 30;  // 中等平滑时间
result = MC_SetJogPrm(3, &jogPrm);

printf("JOG运动参数配置完成\n");
```

**预期输出**:
```
配置JOG运动参数...
轴1 JGO运动参数设置成功
  最大速度: 3.00 脉冲/ms
  加速度: 0.20 脉冲/ms²
  减速度: 0.20 脉冲/ms²
  平滑时间: 50 ms
JOG运动参数配置完成
```

**异常处理机制**:
- 检查结构体指针有效性
- 验证参数值合理性
- 确认轴已设置为JOG模式

### MC_SetJogPrmSingle

**函数原型**:
```c
int MC_SetJogPrmSingle(short nAxisNum, double dAcc, double dDec, double dMaxVel, short dSmoothTime);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `dAcc` [输入]: 加速度
  - 类型: double
  - 范围: >0
  - 单位: 脉冲/ms²
- `dDec` [输入]: 减速度
  - 类型: double
  - 范围: >0
  - 单位: 脉冲/ms²
- `dMaxVel` [输入]: 最大速度
  - 类型: double
  - 范围: >0
  - 单位: 脉冲/ms
- `dSmoothTime` [输入]: 平滑时间
  - 类型: short
  - 范围: [0, 65535]
  - 单位: ms

**返回值**: 参见API返回值说明

**功能描述**: 单个参数方式设置JOG运动参数，简化参数配置流程。

**典型应用场景**:
- 快速参数调整
- 动态JOG配置
- 简化编程接口

**代码示例**:
```c
int result = 0;

// 使用单个参数方式配置JOG运动
printf("使用单个参数方式配置JOG运动...\n");

// 为轴1设置精密JOG参数
result = MC_SetJogPrmSingle(1, 0.2, 0.2, 3.0, 50);
if (result == 0) {
    printf("轴1精密JOG参数设置成功\n");
    printf("  最大速度: 3.0 脉冲/ms\n");
    printf("  加速度: 0.2 脉冲/ms²\n");
    printf("  平滑时间: 50 ms\n");
}

// 为轴2设置高速JOG参数
result = MC_SetJogPrmSingle(2, 1.5, 1.5, 25.0, 20);
if (result == 0) {
    printf("轴2高速JOG参数设置成功\n");
    printf("  最大速度: 25.0 脉冲/ms\n");
    printf("  加速度: 1.5 脉冲/ms²\n");
    printf("  平滑时间: 20 ms\n");
}

// 为轴3设置标准JOG参数
result = MC_SetJogPrmSingle(3, 0.5, 0.5, 10.0, 30);
if (result == 0) {
    printf("轴3标准JOG参数设置成功\n");
    printf("  最大速度: 10.0 脉冲/ms\n");
    printf("  加速度: 0.5 脉冲/ms²\n");
    printf("  平滑时间: 30 ms\n");
}

printf("JOG参数配置完成\n");

// 比较不同轴的JOG特性
printf("\n各轴JOG特性对比:\n");
double jogMaxVels[] = {3.0, 25.0, 10.0};
const char* jogTypes[] = {"精密", "高速", "标准"};

for (int i = 1; i <= 3; i++) {
    printf("轴%d: %s (%.1f 脉冲/ms)\n", i, jogTypes[i-1], jogMaxVels[i-1]);
}
```

**预期输出**:
```
使用单个参数方式配置JOG运动...
轴1精密JOG参数设置成功
  最大速度: 3.0 脉冲/ms
  加速度: 0.2 脉冲/ms²
  平滑时间: 50 ms
轴2高速JOG参数设置成功
  最大速度: 25.0 脉冲/ms
  加速度: 1.5 脉冲/ms²
  平滑时间: 20 ms
轴3标准JOG参数设置成功
  最大速度: 10.0 脉冲/ms
  加速度: 0.5 脉冲/ms²
  平滑时间: 30 ms
JOG参数配置完成

各轴JOG特性对比:
轴1: 精密 (3.0 脉冲/ms)
轴2: 高速 (25.0 脉冲/ms)
轴3: 标准 (10.0 脉冲/ms)
```

**异常处理机制**:
- 检查加速度和减速度是否为正值
- 验证最大速度合理性
- 确认平滑时间在有效范围内

## 2.3 JOG运动控制

### MC_JogPositive

**函数原型**:
```c
int MC_JogPositive(short nAxisNum);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 启动指定轴的正向JOG运动，轴按配置的速度向正方向连续运动。

**典型应用场景**:
- 正向手动调试
- 正向点动操作
- 正向位置调整

**代码示例**:
```c
int result = 0;

// 先设置轴1为JOG模式并配置参数
result = MC_JogOn(1);
result += MC_SetJogPrmSingle(1, 0.5, 0.5, 5.0, 30);

if (result != 0) {
    printf("轴1 JGO配置失败，错误码: %d\n", result);
    return -1;
}

// 启动轴1的正向JOG运动
printf("启动轴1正向JOG运动...\n");
result = MC_JogPositive(1);
if (result == 0) {
    printf("轴1正向JOG运动已启动\n");
    printf("速度: 5.0 脉冲/ms\n");
    printf("按任意键停止...\n");

    // 监控运动状态
    while (!_kbhit()) {
        double pos = 0;
        MC_GetPrfPos(1, &pos);
        printf("\r当前位置: %.1f 脉冲", pos);
        Sleep(100);
    }

    // 停止JOG运动
    MC_JogOff(1);
    printf("\n轴1 JOG运动已停止\n");
} else {
    printf("启动轴1正向JOG运动失败，错误码: %d\n", result);
}
```

**预期输出**:
```
启动轴1正向JOG运动...
轴1正向JOG运动已启动
速度: 5.0 脉冲/ms
按任意键停止...
当前位置: 1250.3 脉冲
轴1 JGO运动已停止
```

**异常处理机制**:
- 确认轴已设置为JOG模式
- 检查正限位状态
- 验证轴使能状态

### MC_JogNegative

**函数原型**:
```c
int MC_JogNegative(short nAxisNum);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 启动指定轴的负向JOG运动，轴按配置的速度向负方向连续运动。

**典型应用场景**:
- 负向手动调试
- 负向点动操作
- 负向位置调整

**代码示例**:
```c
int result = 0;

// 多轴JOG运动演示
printf("多轴JOG运动演示...\n");

// 配置轴1和轴2的JOG参数
MC_JogOn(1);
MC_JogOn(2);
MC_SetJogPrmSingle(1, 0.3, 0.3, 3.0, 20);  // 轴1：精密JOG
MC_SetJogPrmSingle(2, 0.8, 0.8, 15.0, 25); // 轴2：标准JOG

// 启动不同方向的JOG运动
printf("轴1正向JOG运动（精密模式）\n");
result = MC_JogPositive(1);
if (result == 0) {
    printf("轴2负向JOG运动（标准模式）\n");
    result = MC_JogNegative(2);

    if (result == 0) {
        printf("双轴JOG运动已启动\n");

        // 监控运动状态5秒
        for (int i = 0; i < 50; i++) {
            double pos1 = 0, pos2 = 0;
            MC_GetPrfPos(1, &pos1);
            MC_GetPrfPos(2, &pos2);
            printf("\r轴1: %+.1f, 轴2: %+.1f", pos1, pos2);
            Sleep(100);
        }

        // 停止所有JOG运动
        MC_JogStop(0x03);
        printf("\n\n双轴JOG运动已停止\n");
    }
}
```

**预期输出**:
```
多轴JOG运动演示...
轴1正向JOG运动（精密模式）
轴2负向JOG运动（标准模式）
双轴JOG运动已启动
轴1: +150.2, 轴2: -300.5

双轴JOG运动已停止
```

**异常处理机制**:
- 确认轴已设置为JOG模式
- 检查负限位状态
- 验证轴使能状态

### MC_ChangeVel

**函数原型**:
```c
int MC_ChangeVel(short nAxisNum, double dVel);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `dVel` [输入]: 新的目标速度
  - 类型: double
  - 范围: 非零值
  - 单位: 脉冲/ms

**返回值**: 参见API返回值说明

**功能描述**: 动态改变JOG运动的速度，支持运行时速度调整。

**典型应用场景**:
- 实时速度调节
- 分段速度控制
- 平滑速度过渡

**代码示例**:
```c
int result = 0;

// 演示动态速度调节
printf("动态速度调节演示...\n");

// 配置并启动轴1的JOG运动
MC_JogOn(1);
MC_SetJogPrmSingle(1, 0.5, 0.5, 10.0, 30);

printf("启动轴1JOG运动，初始速度: 10.0 脉冲/ms\n");
result = MC_JogPositive(1);

if (result == 0) {
    double speeds[] = {2.0, 5.0, 8.0, 15.0, 3.0, 0.0};

    for (int i = 0; i < 6; i++) {
        Sleep(2000); // 运行2秒

        printf("调整速度到: %.1f 脉冲/ms\n", speeds[i]);
        result = MC_ChangeVel(1, speeds[i]);
        if (result == 0) {
            printf("✓ 速度调整成功\n");
        } else {
            printf("✗ 速度调整失败，错误码: %d\n", result);
            break;
        }
    }

    // 停止运动
    MC_JogOff(1);
    printf("JOG运动演示完成\n");
}
```

**预期输出**:
```
动态速度调节演示...
启动轴1JOG运动，初始速度: 10.0 脉冲/ms
调整速度到: 2.0 脉冲/ms
✓ 速度调整成功
调整速度到: 5.0 脉冲/ms
✓ 速度调整成功
调整速度到: 8.0 脉冲/ms
✓ 速度调整成功
调整速度到: 15.0 脉冲/ms
✓ 速度调整成功
调整速度到: 3.0 脉冲/ms
✓ 速度调整成功
调整速度到: 0.0 脉冲/ms
✓ 速度调整成功
JOG运动演示完成
```

**异常处理机制**:
- 确认轴处于JOG运动状态
- 检查速度值有效性
- 验证速度变化范围合理

---

