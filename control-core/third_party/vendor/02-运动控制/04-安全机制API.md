# 4. 安全机制API

安全机制API提供完善的安全保护功能，确保系统和操作人员的安全。

## 4.1 急停控制

### MC_EmgStop

**函数原型**:
```c
int MC_EmgStop(long axisMask);
```

**参数说明**:
- `axisMask` [输入]: 急停轴掩码
  - 类型: long
  - 范围: 0-4294967295
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 执行急停操作，立即停止指定轴的所有运动。

**位定义说明**:
- Bit0: 急停轴1
- Bit1: 急停轴2
- Bit2: 急停轴3
- ...

**典型应用场景**:
- 紧急情况停止
- 安全保护触发
- 故障急停

**代码示例**:
```c
int result = 0;

// 急停演示
printf("急停控制演示...\n");

// 模拟正常运动
printf("启动正常运动...\n");
MC_PrfTrap(1);
MC_SetTrapPrmSingle(1, 1.0, 1.0, 0.0, 0);
MC_SetPos(1, 10000);
MC_SetVel(1, 10.0);
MC_Update(0x01);

// 运行3秒后触发急停
Sleep(3000);

printf("触发急停！\n");
result = MC_EmgStop(0x01);  // 急停轴1

if (result == 0) {
    printf("✓ 急停操作执行成功\n");

    // 检查急停状态
    long status = 0;
    MC_GetSts(1, &status);
    if (status & 0x1000) {
        printf("✓ 急停状态已激活\n");
    }

    // 清除急停状态
    printf("清除急停状态...\n");
    MC_ClrSts(1, 0x1000);  // 清除急停状态位

    // 重新使能轴
    MC_AxisOn(1);
    printf("轴已重新使能\n");
} else {
    printf("✗ 急停操作失败，错误码: %d\n", result);
}

printf("急停控制演示完成\n");
```

**预期输出**:
```
急停控制演示...
启动正常运动...
触发急停！
✓ 急停操作执行成功
✓ 急停状态已激活
清除急停状态...
轴已重新使能
急停控制演示完成
```

**异常处理机制**:
- 检查轴掩码有效性
- 确认急停权限
- 验证硬件急停线路

### MC_AlmStsReset

**函数原型**:
```c
int MC_AlmStsReset(long axisMask);
```

**参数说明**:
- `axisMask` [输入]: 复位报警状态轴掩码
  - 类型: long
  - 范围: 0-4294967295
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 复位指定轴的报警状态，清除报警标志。

**典型应用场景**:
- 报警状态复位
- 故障恢复后复位
- 系统重启后复位

**代码示例**:
```c
int result = 0;

// 报警状态复位演示
printf("报警状态复位演示...\n");

// 检查初始报警状态
long initialStatus = 0;
MC_GetSts(1, &initialStatus);
printf("初始报警状态: 0x%04lX\n", initialStatus);

// 模拟报警状态
printf("模拟触发报警...\n");
// 这里只是示例，实际报警由硬件触发

// 复位报警状态
result = MC_AlmStsReset(0x0F);  // 复位轴1-4的报警状态

if (result == 0) {
    printf("✓ 报警状态已复位\n");

    // 检查复位后的状态
    long newStatus = 0;
    MC_GetSts(1, &newStatus);
    printf("复位后状态: 0x%04lX\n", newStatus);

    if ((newStatus & 0x006) == 0) {
        printf("✓ 驱动报警和伺服报警已清除\n");
    }
} else {
    printf("✗ 报警状态复位失败，错误码: %d\n", result);
}

printf("报警状态复位演示完成\n");
```

**预期输出**:
```
报警状态复位演示...
初始报警状态: 0x0000
模拟触发报警...
✓ 报警状态已复位
复位后状态: 0x0000
✓ 驱动报警和伺服报警已清除
报警状态复位演示完成
```

**异常处理机制**:
- 检查轴掩码有效性
- 确认复位权限
- 验证硬件连接

## 4.2 安全门控制

### MC_SafeDoorOpen

**函数开门操作，启用安全门状态。

**典型应用场景**:
- 安全门打开检测
- 设备维护时启用
- 安全区域进入

**代码示例**:
```c
int result = 0;

// 安全门控制演示
printf("安全门控制演示...\n");

// 检查安全门状态
printf("检测安全门状态...\n");
// 这里只是示例，实际状态由硬件检测

// 开启安全门
printf("开启安全门模式...\n");
result = MC_SafeDoorOpen(1);

if (result == 0) {
    printf("✓ 安全门模式已启用\n");
    printf("警告：安全门已打开，请注意安全\n");

    // 检查安全门状态
    long status = 0;
    MC_GetSts(1, &status);
    if (status & 0x04000) { // 假设安全门状态位
        printf("✓ 安全门状态已检测到\n");
    }

    // 在安全门打开状态下限制某些操作
    printf("安全限制：高速运动已禁用\n");
    MC_SetVel(1, 5.0);  // 限制速度
} else {
    printf("✗ 安全门启用失败，错误码: %d\n", result);
}

printf("安全门控制演示完成\n");
```

**预期输出**:
```
安全门控制演示...
检测安全门状态...
开启安全门模式...
✓ 安全门模式已启用
警告：安全门已打开，请注意安全
✓ 安全门状态已检测到
安全限制：高速运动已禁用
安全门控制演示完成
```

**异常处理机制**:
- 检查硬件安全门连接
- 验证安全门状态
- 确认操作权限

### MC_SafeDoorClose

**函数原型**:
```c
int MC_SafeDoorClose(short nAxisNum);
```

**参数说明**:
- `nAxisNum` [输入]: 规划轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 关闭安全门模式，恢复正常操作权限。

**典型应用场景**:
- 安全门关闭检测
- 设备恢复正常运行
- 安全保护解除

**代码示例**:
```c
int result = 0;

// 安全门关闭演示
printf("安全门关闭演示...\n");

// 关闭安全门模式
result = MC_SafeDoorClose(1);

if (result == 0) {
    printf("✓ 安全门模式已关闭\n");
    printf("系统恢复正常运行权限\n");

    // 恢复正常速度
    MC_SetVel(1, 20.0);  // 恢复高速运动
    printf("高速运动权限已恢复\n");

    // 检查状态
    long status = 0;
    MC_GetSts(1, &status);
    printf("当前状态: 0x%04lX\n", status);
} else {
    printf("✗ 安全门关闭失败，错误码: %d\n", result);
}

printf("安全门关闭演示完成\n");
```

**预期输出**:
```
安全门关闭演示...
✓ 安全门模式已关闭
系统恢复正常运行权限
高速运动权限已恢复
当前状态: 0x1A00
安全门关闭演示完成
```

**异常处理机制**:
- 检查安全门硬件状态
- 确认安全门完全关闭
- 验证操作权限

---

