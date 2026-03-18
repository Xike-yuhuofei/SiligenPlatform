## 2.4 点位运动API

点位运动API用于控制轴进行精确的位置移动，支持加减速曲线控制和多种运动模式。

### MC_PrfTrap

**函数原型**:
```c
int MC_PrfTrap(short nAxisNum);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置指定轴为点位运动模式，为后续的点位运动做准备。

**典型应用场景**:
- 位置定位应用
- 精确移动控制
- 自动化装配
- 坐标定位

**代码示例**:
```c
int result = 0;

// 设置轴1-4为点位运动模式
printf("设置点位运动模式...\n");

for (int axis = 1; axis <= 4; axis++) {
    result = MC_PrfTrap(axis);
    if (result == 0) {
        printf("轴%d已设置为点位运动模式\n", axis);
    } else {
        printf("轴%d设置点位模式失败，错误码: %d\n", axis, result);
    }
}

printf("点位运动模式设置完成\n");
```

**预期输出**:
```
设置点位运动模式...
轴1已设置为点位运动模式
轴2已设置为点位运动模式
轴3已设置为点位运动模式
轴4已设置为点位运动模式
点位运动模式设置完成
```

**异常处理机制**:
- 检查轴号范围
- 确认轴未被其他运动模式占用
- 验证轴状态正常

### MC_SetTrapPrm

**函数原型**:
```c
int MC_SetTrapPrm(short nAxisNum, TTrapPrm *pPrm);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `pPrm` [输入]: 点位模式运动参数结构体指针
  - 类型: TTrapPrm*
  - 包含: 加速度、减速度、起始速度、平滑时间

**结构体定义**:
```c
typedef struct TrapPrm {
    double acc;        // 加速度，单位：脉冲/ms²
    double dec;        // 减速度，单位：脉冲/ms²
    double velStart;   // 起始速度，固定为0
    short smoothTime;  // 平滑时间，固定为0
} TTrapPrm;
```

**返回值**: 参见API返回值说明

**功能描述**: 设置点位模式的运动参数，包括加速度、减速度等关键参数。

**典型应用场景**:
- 运动参数配置
- 不同负载的运动优化
- 精确控制需求

**代码示例**:
```c
int result = 0;
TTrapPrm trapPrm;

// 配置点位运动参数
printf("配置点位运动参数...\n");

// 初始化参数结构体
memset(&trapPrm, 0, sizeof(trapPrm));

// 设置运动参数
trapPrm.acc = 0.5;        // 加速度：0.5脉冲/ms²
trapPrm.dec = 0.5;        // 减速度：0.5脉冲/ms²
trapPrm.velStart = 0.0;   // 起始速度：固定为0
trapPrm.smoothTime = 0;   // 平滑时间：固定为0

// 为轴1设置点位参数
result = MC_SetTrapPrm(1, &trapPrm);
if (result == 0) {
    printf("轴1点位运动参数设置成功\n");
    printf("  加速度: %.2f 脉冲/ms²\n", trapPrm.acc);
    printf("  减速度: %.2f 脉冲/ms²\n", trapPrm.dec);
} else {
    printf("轴1参数设置失败，错误码: %d\n", result);
}

// 为不同轴设置不同参数
trapPrm.acc = 1.0;  // 轴2使用更高加速度
trapPrm.dec = 1.0;
result = MC_SetTrapPrm(2, &trapPrm);

trapPrm.acc = 0.2;  // 轴3使用更低加速度（精密运动）
trapPrm.dec = 0.2;
result = MC_SetTrapPrm(3, &trapPrm);

trapPrm.acc = 0.8;  // 轴4使用中等加速度
trapPrm.dec = 0.8;
result = MC_SetTrapPrm(4, &trapPrm);

printf("点位运动参数配置完成\n");
```

**预期输出**:
```
配置点位运动参数...
轴1点位运动参数设置成功
  加速度: 0.50 脉冲/ms²
  减速度: 0.50 脉冲/ms²
点位运动参数配置完成
```

**异常处理机制**:
- 检查结构体指针有效性
- 验证参数值合理性
- 确认轴已设置为点位模式

### MC_SetTrapPrmSingle

**函数原型**:
```c
int MC_SetTrapPrmSingle(short nAxisNum, double dAcc, double dDec, double dVelStart, short dSmoothTime);
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
- `dVelStart` [输入]: 起始速度
  - 类型: double
  - 范围: 固定为0
  - 单位: 脉冲/ms
- `dSmoothTime` [输入]: 平滑时间
  - 类型: short
  - 范围: 固定为0
  - 单位: ms

**返回值**: 参见API返回值说明

**功能描述**: 单个参数方式设置点位运动参数，可替代MC_SetTrapPrm函数，便于不熟悉结构体的开发者使用。

**典型应用场景**:
- 简化参数设置
- 动态参数调整
- 快速配置应用

**代码示例**:
```c
int result = 0;

// 使用单个参数方式配置点位运动
printf("使用单个参数方式配置点位运动...\n");

// 为轴1设置点位参数
result = MC_SetTrapPrmSingle(1, 0.5, 0.5, 0.0, 0);
if (result == 0) {
    printf("轴1点位参数设置成功\n");
    printf("  加速度: 0.5 脉冲/ms²\n");
    printf("  减速度: 0.5 脉冲/ms²\n");
}

// 为轴2设置高速参数
result = MC_SetTrapPrmSingle(2, 2.0, 2.0, 0.0, 0);
if (result == 0) {
    printf("轴2高速参数设置成功 (2.0 脉冲/ms²)\n");
}

// 为轴3设置精密参数
result = MC_SetTrapPrmSingle(3, 0.1, 0.1, 0.0, 0);
if (result == 0) {
    printf("轴3精密参数设置成功 (0.1 脉冲/ms²)\n");
}

// 为轴4设置标准参数
result = MC_SetTrapPrmSingle(4, 1.0, 1.0, 0.0, 0);
if (result == 0) {
    printf("轴4标准参数设置成功 (1.0 脉冲/ms²)\n");
}

printf("点位参数配置完成\n");

// 比较不同轴的加速能力
printf("\n各轴加速能力对比:\n");
double accelerations[] = {0.5, 2.0, 0.1, 1.0};
const char* descriptions[] = {"标准", "高速", "精密", "中速"};

for (int i = 1; i <= 4; i++) {
    printf("轴%d: %s (%.1f 脉冲/ms²)\n", i, descriptions[i-1], accelerations[i-1]);
}
```

**预期输出**:
```
使用单个参数方式配置点位运动...
轴1点位参数设置成功
  加速度: 0.5 脉冲/ms²
  减速度: 0.5 脉冲/ms²
轴2高速参数设置成功 (2.0 脉冲/ms²)
轴3精密参数设置成功 (0.1 脉冲/ms²)
轴4标准参数设置成功 (1.0 脉冲/ms²)
点位参数配置完成

各轴加速能力对比:
轴1: 标准 (0.5 脉冲/ms²)
轴2: 高速 (2.0 脉冲/ms²)
轴3: 精密 (0.1 脉冲/ms²)
轴4: 中速 (1.0 脉冲/ms²)
```

**异常处理机制**:
- 检查加速度和减速度是否为正值
- 验证轴号范围
- 确认起始速度和平滑时间参数正确

### MC_SetPos

**函数原型**:
```c
int MC_SetPos(short nAxisNum, long pos);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `pos` [输入]: 目标位置
  - 类型: long
  - 范围: [-2147483648, 2147483647]
  - 单位: 脉冲

**返回值**: 参见API返回值说明

**功能描述**: 设置轴的目标位置，用于点位运动的位移控制。

**典型应用场景**:
- 绝对位置定位
- 坐标移动
- 位置调整

**代码示例**:
```c
int result = 0;

// 设置轴的目标位置
printf("设置轴目标位置...\n");

// 为轴1设置正向目标位置
long targetPos1 = 10000;
result = MC_SetPos(1, targetPos1);
if (result == 0) {
    printf("轴1目标位置: %ld 脉冲\n", targetPos1);
}

// 为轴2设置负向目标位置
long targetPos2 = -5000;
result = MC_SetPos(2, targetPos2);
if (result == 0) {
    printf("轴2目标位置: %ld 脉冲\n", targetPos2);
}

// 为轴3设置原点位置
result = MC_SetPos(3, 0);
if (result == 0) {
    printf("轴3目标位置: 0 脉冲 (原点)\n");
}

// 为轴4设置大范围位置
long targetPos4 = 50000;
result = MC_SetPos(4, targetPos4);
if (result == 0) {
    printf("轴4目标位置: %ld 脉冲\n", targetPos4);
}

printf("目标位置设置完成\n");

// 显示位置规划信息
printf("\n位置规划信息:\n");
long positions[] = {targetPos1, targetPos2, 0, targetPos4};
for (int i = 1; i <= 4; i++) {
    printf("轴%d: %ld 脉冲", i, positions[i-1]);
    if (positions[i-1] > 0) {
        printf(" (正向移动)");
    } else if (positions[i-1] < 0) {
        printf(" (负向移动)");
    } else {
        printf(" (原点)");
    }
    printf("\n");
}
```

**预期输出**:
```
设置轴目标位置...
轴1目标位置: 10000 脉冲
轴2目标位置: -5000 脉冲
轴3目标位置: 0 脉冲 (原点)
轴4目标位置: 50000 脉冲
目标位置设置完成

位置规划信息:
轴1: 10000 脉冲 (正向移动)
轴2: -5000 脉冲 (负向移动)
轴3: 0 脉冲 (原点)
轴4: 50000 脉冲 (正向移动)
```

**异常处理机制**:
- 检查位置值范围
- 验证轴号有效性
- 确认轴处于可设置状态

### MC_SetVel

**函数原型**:
```c
int MC_SetVel(short nAxisNum, double vel);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `vel` [输入]: 目标速度
  - 类型: double
  - 范围: 非零值
  - 单位: 脉冲/ms

**返回值**: 参见API返回值说明

**功能描述**: 设置轴的目标速度，控制点位运动的运行速度。

**典型应用场景**:
- 运动速度控制
- 不同工序的速度调整
- 精密与快速运动切换

**代码示例**:
```c
int result = 0;

// 设置轴的运动速度
printf("设置轴运动速度...\n");

// 为轴1设置中速
double vel1 = 10.0;
result = MC_SetVel(1, vel1);
if (result == 0) {
    printf("轴1速度: %.2f 脉冲/ms (%.1f 转/秒)\n", vel1, vel1 * 1000 / 10000);
}

// 为轴2设置高速
double vel2 = 50.0;
result = MC_SetVel(2, vel2);
if (result == 0) {
    printf("轴2速度: %.2f 脉冲/ms (%.1f 转/秒)\n", vel2, vel2 * 1000 / 10000);
}

// 为轴3设置低速（精密运动）
double vel3 = 2.0;
result = MC_SetVel(3, vel3);
if (result == 0) {
    printf("轴3速度: %.2f 脉冲/ms (%.1f 转/秒)\n", vel3, vel3 * 1000 / 10000);
}

// 为轴4设置负向速度
double vel4 = -15.0;
result = MC_SetVel(4, vel4);
if (result == 0) {
    printf("轴4速度: %.2f 脉冲/ms (负向)\n", vel4);
}

printf("速度设置完成\n");

// 速度对比分析
printf("\n速度特性分析:\n");
double velocities[] = {vel1, vel2, vel3, vel4};
const char* speedTypes[] = {"中速", "高速", "精密", "中速负向"};

for (int i = 1; i <= 4; i++) {
    double rpm = fabs(velocities[i-1]) * 1000 / 10000; // 假设10000脉冲/转
    printf("轴%d: %s (%.2f 脉冲/ms, %.1f RPM)\n",
           i, speedTypes[i-1], velocities[i-1], rpm);
}

// 计算理论运动时间
printf("\n移动10000脉冲的理论时间:\n");
for (int i = 1; i <= 4; i++) {
    double time = 10000.0 / fabs(velocities[i-1]); // 毫秒
    printf("轴%d: %.2f 秒\n", i, time / 1000);
}
```

**预期输出**:
```
设置轴运动速度...
轴1速度: 10.00 脉冲/ms (1.0 转/秒)
轴2速度: 50.00 脉冲/ms (5.0 转/秒)
轴3速度: 2.00 脉冲/ms (0.2 转/秒)
轴4速度: -15.00 脉冲/ms (负向)
速度设置完成

速度特性分析:
轴1: 中速 (10.00 脉冲/ms, 1.0 RPM)
轴2: 高速 (50.00 脉冲/ms, 5.0 RPM)
轴3: 精密 (2.00 脉冲/ms, 0.2 RPM)
轴4: 中速负向 (-15.00 脉冲/ms, 1.5 RPM)

移动10000脉冲的理论时间:
轴1: 1.00 秒
轴2: 0.20 秒
轴3: 5.00 秒
轴4: 0.67 秒
```

**异常处理机制**:
- 检查速度值是否为非零
- 验证速度范围合理性
- 确认轴处于可设置状态

### MC_Update

**函数原型**:
```c
int MC_Update(long mask);
```

**参数说明**:
- `mask` [输入]: 按位指示需要启动点位运动的轴号
  - 类型: long
  - 范围: 0-4294967295
  - 单位: 无

**位定义说明**:
- Bit0: 启动轴1点位运动
- Bit1: 启动轴2点位运动
- Bit2: 启动轴3点位运动
- ...
- Bit31: 启动轴32点位运动

**返回值**: 参见API返回值说明

**功能描述**: 启动指定轴的点位运动，可以同时启动多个轴的运动。

**典型应用场景**:
- 多轴同步运动
- 批量运动启动
- 协调控制应用

**代码示例**:
```c
int result = 0;

// 启动点位运动
printf("启动点位运动...\n");

// 启动轴1的运动 (Bit0 = 1)
result = MC_Update(0x0001);
if (result == 0) {
    printf("轴1运动已启动\n");
}

// 启动轴2和轴3的运动 (Bit1=1, Bit2=1)
result = MC_Update(0x0006);
if (result == 0) {
    printf("轴2和轴3运动已启动\n");
}

// 启动轴4的运动 (Bit3=1)
result = MC_Update(0x0008);
if (result == 0) {
    printf("轴4运动已启动\n");
}

// 同时启动多个轴
printf("\n同时启动多个轴测试:\n");

// 启动轴1、轴3、轴5 (假设有5轴)
result = MC_Update(0x0015); // 0b0000000000010101
if (result == 0) {
    printf("轴1、轴3、轴5同步运动已启动\n");
    printf("掩码值: 0x%04lX\n", 0x0015);
}

// 显示位掩码对应关系
printf("\n位掩码对应关系:\n");
for (int i = 0; i < 8; i++) {
    long mask = 1 << i;
    printf("Bit%d (0x%02lX): 启动轴%d\n", i, mask, i + 1);
}

// 组合运动示例
printf("\n组合运动示例:\n");
long combinations[] = {0x0003, 0x000C, 0x00F0, 0xFF00};
const char* descriptions[] = {"轴1-2", "轴3-4", "轴5-8", "轴9-16"};

for (int i = 0; i < 4; i++) {
    printf("掩码0x%04lX: %s\n", combinations[i], descriptions[i]);
}

printf("点位运动启动完成\n");
```

**预期输出**:
```
启动点位运动...
轴1运动已启动
轴2和轴3运动已启动
轴4运动已启动

同时启动多个轴测试:
轴1、轴3、轴5同步运动已启动
掩码值: 0x0015

位掩码对应关系:
Bit0 (0x01): 启动轴1
Bit1 (0x02): 启动轴2
Bit2 (0x04): 启动轴3
Bit3 (0x08): 启动轴4
Bit4 (0x10): 启动轴5
Bit5 (0x20): 启动轴6
Bit6 (0x40): 启动轴7
Bit7 (0x80): 启动轴8

组合运动示例:
掩码0x0003: 轴1-2
掩码0x000C: 轴3-4
掩码0x00F0: 轴5-8
掩码0xFF00: 轴9-16
点位运动启动完成
```

**异常处理机制**:
- 检查轴参数是否已配置
- 验证轴状态是否允许启动
- 确认没有轴正在执行其他运动模式

### MC_SetTrapPosAndUpdate

**函数原型**:
```c
int MC_SetTrapPosAndUpdate(short nAxisNum, long long llPos, double dVel, double dAcc, double dDec, double dVelStart, short nSmoothTime, short nBlock);
```

**参数说明**:
- `nAxisNum` [输入]: 轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `llPos` [输入]: 目标位置
  - 类型: long long
  - 范围: ±9.22×10¹⁸
  - 单位: 脉冲
- `dVel` [输入]: 速度
  - 类型: double
  - 范围: 非零
  - 单位: 脉冲/ms
- `dAcc` [输入]: 加速度
  - 类型: double
  - 范围: >0
  - 单位: 脉冲/ms²
- `dDec` [输入]: 减速度
  - 类型: double
  - 范围: >0
  - 单位: 脉冲/ms²
- `dVelStart` [输入]: 起始速度
  - 类型: double
  - 范围: 固定为0
  - 单位: 脉冲/ms
- `nSmoothTime` [输入]: 平滑时间
  - 类型: short
  - 范围: 固定为0
  - 单位: ms
- `nBlock` [输入]: 阻塞标志
  - 类型: short
  - 范围: 固定为0
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 一体化点位运动函数，设置参数并立即启动运动，提供高效的运动控制方式。

**典型应用场景**:
- 快速点位运动
- 简化运动控制流程
- 高效运动序列

**代码示例**:
```c
int result = 0;

// 一体化点位运动测试
printf("开始一体化点位运动测试...\n");

// 轴1：标准运动
result = MC_SetTrapPosAndUpdate(1, 10000, 10.0, 0.5, 0.5, 0.0, 0, 0);
if (result == 0) {
    printf("轴1运动已启动\n");
    printf("  目标位置: 10000 脉冲\n");
    printf("  速度: 10.0 脉冲/ms\n");
    printf("  加速度: 0.5 脉冲/ms²\n");
}

// 轴2：高速运动
result = MC_SetTrapPosAndUpdate(2, -8000, 25.0, 2.0, 2.0, 0.0, 0, 0);
if (result == 0) {
    printf("轴2运动已启动\n");
    printf("  目标位置: -8000 脉冲\n");
    printf("  速度: 25.0 脉冲/ms (高速)\n");
    printf("  加速度: 2.0 脉冲/ms²\n");
}

// 轴3：精密运动
result = MC_SetTrapPosAndUpdate(3, 5000, 3.0, 0.1, 0.1, 0.0, 0, 0);
if (result == 0) {
    printf("轴3运动已启动\n");
    printf("  目标位置: 5000 脉冲\n");
    printf("  速度: 3.0 脉冲/ms (精密)\n");
    printf("  加速度: 0.1 脉冲/ms²\n");
}

// 轴4：大范围运动
result = MC_SetTrapPosAndUpdate(4, 50000, 50.0, 1.5, 1.5, 0.0, 0, 0);
if (result == 0) {
    printf("轴4运动已启动\n");
    printf("  目标位置: 50000 脉冲\n");
    printf("  速度: 50.0 脉冲/ms\n");
    printf("  加速度: 1.5 脉冲/ms²\n");
}

printf("\n运动特性分析:\n");

struct MotionProfile {
    long long position;
    double velocity;
    double acceleration;
    const char* type;
};

MotionProfile profiles[] = {
    {10000, 10.0, 0.5, "标准"},
    {-8000, 25.0, 2.0, "高速"},
    {5000, 3.0, 0.1, "精密"},
    {50000, 50.0, 1.5, "大范围"}
};

for (int i = 1; i <= 4; i++) {
    MotionProfile* p = &profiles[i-1];
    double totalTime = (double)abs(p->position) / p->velocity; // 简化计算
    printf("轴%d (%s): 位置=%lld, 速度=%.1f, 时间≈%.2fs\n",
           i, p->type, p->position, p->velocity, totalTime);
}

printf("一体化点位运动测试完成\n");
```

**预期输出**:
```
开始一体化点位运动测试...
轴1运动已启动
  目标位置: 10000 脉冲
  速度: 10.0 脉冲/ms
  加速度: 0.5 脉冲/ms²
轴2运动已启动
  目标位置: -8000 脉冲
  速度: 25.0 脉冲/ms (高速)
  加速度: 2.0 脉冲/ms²
轴3运动已启动
  目标位置: 5000 脉冲
  速度: 3.0 脉冲/ms (精密)
  加速度: 0.1 脉冲/ms²
轴4运动已启动
  目标位置: 50000 脉冲
  速度: 50.0 脉冲/ms
  加速度: 1.5 脉冲/ms²

运动特性分析:
轴1 (标准): 位置=10000, 速度=10.0, 时间≈1.00s
轴2 (高速): 位置=-8000, 速度=25.0, 时间≈0.32s
轴3 (精密): 位置=5000, 速度=3.0, 时间≈1.67s
轴4 (大范围): 位置=50000, 速度=50.0, 时间≈1.00s
一体化点位运动测试完成
```

**异常处理机制**:
- 检查所有参数的合理性
- 验证轴状态可用性
- 确认运动参数不会导致冲突

---

