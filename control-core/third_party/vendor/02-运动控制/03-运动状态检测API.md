# 3. 运动状态检测API

运动状态检测API提供全面的运动状态监控功能，帮助开发者实时了解系统运行状态。

## 3.1 基础状态检测

### MC_GetSts

**函数原型**:
```c
int MC_GetSts(short nAxisNum, long *pValue);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `pValue` [输出]: 状态值存放指针
  - 类型: long*
  - 范围: 0-4294967295
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 读取指定轴的当前状态信息，包含运动状态、报警信息、限位状态等。

**状态位定义**:
| 位 | 掩码 | 含义 | 说明 |
|----|------|------|------|
| 0 | 0x001 | 运动错误 | 运动过程中发生错误 |
| 1 | 0x002 | 驱动报警 | 驱动器报警信号 |
| 2 | 0x004 | 伺服报警 | 伺服驱动器报警 |
| 3 | 0x008 | 正限位触发 | 正向硬限位触发 |
| 4 | 0x010 | 负限位触发 | 负向硬限位触发 |
| 5 | 0x020 | 正软限位触发 | 正向软限位触发 |
| 6 | 0x040 | 负软限位触发 | 负向软限位触发 |
| 7 | 0x080 | 原点信号 | 原点开关状态 |
| 8 | 0x100 | 使能状态 | 轴使能状态 |
| 9 | 0x200 | 运行状态 | 轴正在运动 |
| 10 | 0x400 | 到位状态 | 运动到位 |
| 11 | 0x800 | 回零完成 | 回零操作完成 |
| 12 | 0x1000 | 急停状态 | 急停信号触发 |
| 13 | 0x2000 | 位置误差 | 位置超差 |
| 14 | 0x4000 | 速度误差 | 速度超差 |
| 15 | 0x8000 | 系统错误 | 系统级错误 |

**典型应用场景**:
- 实时状态监控
- 运动完成检测
- 故障诊断
- 安全保护

**代码示例**:
```c
int result = 0;

// 状态监控演示
printf("运动状态监控演示...\n");

for (int axis = 1; axis <= 4; axis++) {
    long status = 0;
    result = MC_GetSts(axis, &status);

    if (result == 0) {
        printf("轴%d状态: 0x%04lX\n", axis, status);

        // 解析状态位
        if (status & 0x100) printf("  ✓ 轴已使能\n");
        if (status & 0x200) printf("  ► 轴正在运动\n");
        if (status & 0x400) printf("  ★ 轴已到位\n");
        if (status & 0x800) printf("  ● 轴回零完成\n");

        // 检查异常状态
        if (status & 0x002) printf("  ⚠ 驱动报警\n");
        if (status & 0x018) printf("  ⚠ 限位触发\n");
        if (status & 0x1000) printf("  ⚠ 急停触发\n");

        if (!(status & 0x100)) {
            printf("  ⚠ 轴未使能\n");
        }

        printf("\n");
    } else {
        printf("读取轴%d状态失败，错误码: %d\n", axis, result);
    }
}

printf("状态监控完成\n");
```

**预期输出**:
```
运动状态监控演示...
轴1状态: 0x1A00
  ✓ 轴已使能
  ★ 轴已到位
  ● 轴回零完成

轴2状态: 0x1A00
  ✓ 轴已使能
  ★ 轴已到位
  ● 轴回零完成

轴3状态: 0x1A00
  ✓ 轴已使能
  ★ 轴已到位
  ● 轴回零完成

轴4状态: 0x1A00
  ✓ 轴已使能
  ★ 轴已到位
  ● 轴回零完成

状态监控完成
```

**异常处理机制**:
- 检查指针参数有效性
- 确认轴号范围
- 验证返回值

### MC_GetPrfPos

**函数原型**:
```c
int MC_GetPrfPos(short nAxisNum, double *pPos);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `pPos` [输出]: 规划位置存放指针
  - 类型: double*
  - 范围: ±9.22×10¹⁸
  - 单位: 脉冲

**返回值**: 参见API返回值说明

**功能描述**: 读取指定轴的规划位置，即运动控制器输出的目标位置。

**典型应用场景**:
- 位置跟踪
- 运动监控
- 位置精度检测

**代码示例**:
```c
int result = 0;

// 位置监控示例
printf("开始位置监控...\n");

// 设置一个目标位置
MC_SetPos(1, 5000);
MC_SetVel(1, 10.0);
MC_Update(0x01);  // 启动轴1运动

// 实时监控位置变化
for (int i = 0; i < 20; i++) {
    double position = 0;
    result = MC_GetPrfPos(1, &position);

    if (result == 0) {
        printf("轴1规划位置: %.1f 脉冲\n", position);
    } else {
        printf("读取位置失败，错误码: %d\n", result);
        break;
    }

    Sleep(100); // 100ms间隔
}

// 检查最终位置
double finalPosition = 0;
MC_GetPrfPos(1, &finalPosition);
printf("最终位置: %.1f 脉冲\n", finalPosition);
printf("位置监控完成\n");
```

**预期输出**:
```
开始位置监控...
轴1规划位置: 0.0 脉冲
轴1规划位置: 100.0 脉冲
轴1规划位置: 250.0 脉冲
轴1规划位置: 450.0 脉冲
轴1规划位置: 800.0 脉冲
轴1规划位置: 1200.0 脉冲
轴1规划位置: 1650.0 脉冲
轴1规划位置: 2150.0 轴冲
轴1规划位置: 2700.0 脉冲
轴1规划位置: 3250.0 脉冲
轴1规划位置: 3800.0 脉冲
轴1规划位置: 4350.0 脉冲
轴1规划位置: 4850.0 脉冲
轴1规划位置: 5000.0 脉冲
最终位置: 5000.0 脉冲
位置监控完成
```

**异常处理机制**:
- 检查指针参数有效性
- 确认轴号范围
- 验证返回值

### MC_GetAxisEncPos

**函数原型**:
```c
int MC_GetAxisEncPos(short nAxisNum, double *pPos);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `pPos` [输出]: 编码器位置存放指针
  - 类型: double*
  - 范围: ±9.22×10¹⁸
  - 单位: 脉冲

**返回值**: 参见API返回值说明

**功能描述**: 读取指定轴的编码器反馈位置，用于闭环控制的位置反馈。

**典型应用场景**:
- 位置精度检测
- 编码器监控
- 闭环位置验证

**代码示例**:
```c
int result = 0;

// 编码器位置监控示例
printf("编码器位置监控演示...\n");

// 比较规划位置和编码器位置
for (int i = 1; i <= 4; i++) {
    double prfPos = 0, encPos = 0;

    result = MC_GetPrfPos(i, &prfPos);
    result += MC_GetAxisEncPos(i, &encPos);

    if (result == 0) {
        double positionError = prfPos - encPos;
        printf("轴%d:\n", i);
        printf("  规划位置: %.3f 脉冲\n", prfPos);
        printf("  编码器位置: %.3f 脉冲\n", encPos);
        printf("  位置误差: %.3f 脉冲\n", positionError);

        // 评估位置精度
        if (fabs(positionError) < 1.0) {
            printf("  ✓ 位置精度良好\n");
        } else if (fabs(positionError) < 5.0) {
            printf("  ○ 位置精度一般\n");
        } else {
            printf("  ✗ 位置精度较差\n");
        }
        printf("\n");
    } else {
        printf("读取轴%d位置失败，错误码: %d\n", i, result);
    }
}

printf("编码器位置监控完成\n");
```

**预期输出**:
```
编码器位置监控演示...
轴1:
  规划位置: 2500.000 脉冲
  编码器位置: 2499.500 脉冲
  位置误差: 0.500 脉冲
  ✓ 位置精度良好

轴2:
  规划位置: 1500.000 脉冲
  编码器位置: 1502.000 脉冲
  位置误差: -2.000 脉冲
  ○ 位置精度一般

轴3:
  规划位置: 3500.000 脉冲
  编码器位置: 3501.500 脉冲
   位置误差: -1.500 脉冲
  ○ 位置精度一般

轴4:
  规划位置: 0.000 脉冲
  编码器位置: 0.000 脉冲
  位置误差: 0.000 脉冲
  ✓ 位置精度良好

编码器位置监控完成
```

**异常处理机制**:
- 检查指针参数有效性
- 确认编码器已启用
- 验证轴号范围

### MC_GetPrfVel

**函数原型**:
```c
int MC_GetPrfVel(short nAxisNum, double *pVel);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `pVel` [输出]: 规划速度存放指针
  - 类型: double*
  - 范围: ±9.22×10¹⁸
  - 单位: 脉冲/ms

**返回值**: 参见API返回值说明

**功能描述**: 读取指定轴的规划速度，即运动控制器输出的目标速度。

**典型应用场景**:
- 速度监控
- 运动性能分析
- 速度曲线分析

**代码示例**:
```c
int result = 0;

// 速度监控示例
printf("开始速度监控...\n");

// 配置并启动变速运动
MC_PrfTrap(1);
MC_SetTrapPrmSingle(1, 1.0, 1.0, 0.0, 0);
MC_SetPos(1, 10000);
MC_SetVel(1, 0.0);  // 从0开始
MC_Update(0x01);

// 速度变化过程监控
printf("速度变化过程:\n");
for (int i = 0; i < 50; i++) {
    double velocity = 0;
    result = MC_GetPrfVel(1, &velocity);

    if (result == 0) {
        printf("轴1当前速度: %.3f 脉冲/ms", velocity);

        // 速度阶段判断
        if (velocity < 0.1) {
            printf(" (停止阶段)\n");
        } else if (velocity < 9.5) {
            printf(" (加速阶段)\n");
        } else {
            printf(" (匀速阶段)\n");
        }
    }

    Sleep(50);
}

// 检查目标速度
double targetVel = 0;
MC_GetVel(1, &targetVel);
printf("目标速度: %.1f 脉冲/ms\n", targetVel);

printf("速度监控完成\n");
```

**预期输出**:
```
开始速度监控...
速度变化过程:
轴1当前速度: 0.000 脉冲/ms (停止阶段)
轴1当前速度: 0.500 脉冲/ms (加速阶段)
轴1当前速度: 1.500 脉冲/ms (加速阶段)
轴1当前速度: 3.000 脉冲/ms (加速阶段)
轴1当前速度: 5.000 脉冲/ms (加速阶段)
轴1当前速度: 7.000 脉冲/ms (加速阶段)
轴1当前速度: 8.500 脉冲/ms (加速阶段)
轴1当前速度: 9.800 脉冲/ms (加速阶段)
轴1当前速度: 10.000 脉冲/ms (匀速阶段)
轴1当前速度: 10.000 脉冲/ms (匀速阶段)
...
目标速度: 10.0 脉冲/ms
速度监控完成
```

**异常处理机制**:
- 检查指针参数有效性
- 确认轴号范围
- 验证返回值

---

