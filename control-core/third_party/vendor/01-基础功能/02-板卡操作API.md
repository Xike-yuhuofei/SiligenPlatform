# 2. 基础函数库文档

## API返回值说明

所有API函数都采用统一的返回值系统：

| 返回值 | 含义 | 处理建议 |
|--------|------|----------|
| 0 | 执行成功 | 正常流程 |
| 1 | 执行失败 | 检查命令执行条件是否满足 |
| 2 | 版本不支持该API | 如有需要，联系厂家升级固件 |
| 7 | 参数错误 | 检查参数是否合理（类型、范围） |
| -1 | 通讯失败 | 检查接线是否牢靠，更换板卡 |
| -6 | 打开控制器失败 | 检查串口名是否正确，是否重复调用MC_Open |
| -7 | 运动控制器无响应 | 检查控制器连接状态，更换板卡 |

## 2.1 板卡打开关闭API

### MC_SetCardNo

**函数原型**:
```c
int MC_SetCardNo(short iCardNum);
```

**参数说明**:
- `iCardNum` [输入]: 将被设置为当前运动控制器的卡号
  - 类型: short
  - 范围: [1, 255]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 切换当前运动控制器卡号，用于多卡并联系统中选择目标板卡。

**典型应用场景**:
- 多卡并联系统中不同板卡的切换
- 特定板卡参数配置

**代码示例**:
```c
int result = 0;

// 切换到第1块板卡
result = MC_SetCardNo(1);
if (result != 0) {
    printf("切换板卡失败，错误码: %d\n", result);
    return -1;
}

// 执行板卡1相关操作
// ...

// 切换到第2块板卡
result = MC_SetCardNo(2);
if (result != 0) {
    printf("切换板卡失败，错误码: %d\n", result);
    return -1;
}

// 执行板卡2相关操作
// ...

printf("板卡切换操作成功\n");
```

**预期输出**:
```
板卡切换操作成功
```

**异常处理机制**:
- 检查返回值是否为0
- 确保卡号在有效范围内[1,255]
- 在调用其他API前必须先调用此函数

### MC_GetCardNo

**函数原型**:
```c
int MC_GetCardNo(short *pCardNum);
```

**参数说明**:
- `pCardNum` [输出]: 读取的当前运动控制器卡号
  - 类型: short*
  - 范围: [1, 255]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 读取当前运动控制器的卡号，用于确认当前操作的板卡。

**典型应用场景**:
- 调试时确认当前板卡
- 多线程环境中同步板卡状态

**代码示例**:
```c
int result = 0;
short currentCard = 0;

// 设置当前板卡为3
result = MC_SetCardNo(3);
if (result != 0) {
    printf("设置板卡失败，错误码: %d\n", result);
    return -1;
}

// 读取当前板卡号
result = MC_GetCardNo(&currentCard);
if (result != 0) {
    printf("读取板卡号失败，错误码: %d\n", result);
    return -1;
}

printf("当前板卡号: %d\n", currentCard);
```

**预期输出**:
```
当前板卡号: 3
```

**异常处理机制**:
- 检查指针参数是否为NULL
- 确保返回值在有效范围内

### MC_Open (网口方式)

**函数原型**:
```c
int MC_Open(short iType, char* cName);
```

**参数说明**:
- `iType` [输入]: 打开方式
  - 类型: short
  - 范围: 0(网口), 1(串口)
  - 单位: 无
- `cName` [输入]: 网络地址
  - 类型: char*
  - 范围: 有效IP地址字符串
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 通过网口打开运动控制卡连接。

**典型应用场景**:
- 建立与运动控制卡的初始连接
- 网络环境下的运动控制系统

**代码示例**:
```c
int result = 0;

// 设置当前板卡为1
result = MC_SetCardNo(1);
if (result != 0) {
    printf("设置板卡失败，错误码: %d\n", result);
    return -1;
}

// 通过网口打开板卡连接
// PC端IP: 192.168.0.200
result = MC_Open(0, "192.168.0.200");
if (result != 0) {
    printf("打开板卡连接失败，错误码: %d\n", result);
    printf("请检查网络连接和IP设置\n");
    return -1;
}

printf("板卡连接成功建立\n");
```

**预期输出**:
```
板卡连接成功建立
```

**异常处理机制**:
- 检查网络连接状态
- 确认IP地址配置正确
- 验证网络设备工作正常
- 避免重复调用MC_Open

### MC_Open (串口方式)

**函数原型**:
```c
int MC_Open(short iType, char* cName);
```

**参数说明**:
- `iType` [输入]: 打开方式
  - 类型: short
  - 范围: 0(网口), 1(串口)
  - 单位: 无
- `cName` [输入]: 串口号
  - 类型: char*
  - 范围: "COM1"-"COM256"
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 通过串口打开运动控制卡连接。

**典型应用场景**:
- 串口通信的简单应用
- 成本敏感的场合

**代码示例**:
```c
int result = 0;

// 设置当前板卡为1
result = MC_SetCardNo(1);
if (result != 0) {
    printf("设置板卡失败，错误码: %d\n", result);
    return -1;
}

// 通过串口打开板卡连接
result = MC_Open(1, "COM1");
if (result != 0) {
    printf("打开串口连接失败，错误码: %d\n", result);
    printf("请检查串口连接和设备管理器\n");
    return -1;
}

printf("串口连接成功建立\n");
```

**预期输出**:
```
串口连接成功建立
```

**异常处理机制**:
- 检查串口是否存在
- 确认串口未被占用
- 验证波特率等参数配置

### MC_Open (高级网口方式)

**函数原型**:
```c
int MC_Open(short nCardNum, char* cPCEthernetIP, unsigned short nPCEthernetPort,
           char* cCardEthernetIP, unsigned short nCardEthernetPort);
```

**参数说明**:
- `nCardNum` [输入]: 卡号，从1开始
  - 类型: short
  - 范围: [1, 255]
  - 单位: 无
- `cPCEthernetIP` [输入]: 电脑IP地址
  - 类型: char*
  - 范围: 有效IP地址字符串
  - 单位: 无
- `nPCEthernetPort` [输入]: 电脑端口号
  - 类型: unsigned short
  - 范围: 0(自动分配) 或 [1024, 65535]
  - 单位: 无
  - 说明: 推荐使用0让系统自动分配，避免端口冲突
- `cCardEthernetIP` [输入]: 板卡IP地址
  - 类型: char*
  - 范围: 有效IP地址字符串
  - 单位: 无
- `nCardEthernetPort` [输入]: 板卡端口号
  - 类型: unsigned short
  - 范围: 0(自动分配) 或 [1024, 65535]
  - 单位: 无
  - 说明: 推荐使用0让系统自动分配，避免端口冲突

**返回值**: 参见API返回值说明

**功能描述**: 高级网口连接方式，支持自定义端口配置。

**典型应用场景**:
- 需要特定端口配置的网络环境
- 多板卡并行连接

**代码示例**:
```c
int result = 0;

// 高级网口连接配置（推荐使用端口0自动分配）
// 卡号1，电脑IP: 192.168.0.200，端口0（系统自动分配）
// 板卡IP: 192.168.0.1，端口0（系统自动分配）
result = MC_Open(1, "192.168.0.200", 0, "192.168.0.1", 0);
if (result != 0) {
    printf("高级网口连接失败，错误码: %d\n", result);
    return -1;
}

printf("高级网口连接成功建立（端口由系统自动分配）\n");
printf("电脑IP: 192.168.0.200:自动分配端口\n");
printf("板卡IP: 192.168.0.1:自动分配端口\n");
```

**预期输出**:
```
高级网口连接成功建立（端口由系统自动分配）
电脑IP: 192.168.0.200:自动分配端口
板卡IP: 192.168.0.1:自动分配端口
```

**异常处理机制**:
- 确保IP地址在同一网段
- 检查端口是否被占用
- 验证网络防火墙设置

### MC_OpenByIP

**函数原型**:
```c
int MC_OpenByIP(char* cPCIP, char* cCardIP, unsigned long ulID, unsigned short nRetryTime);
```

**参数说明**:
- `cPCIP` [输入]: 电脑IP地址
  - 类型: char*
  - 范围: 有效IP地址字符串
  - 单位: 无
- `cCardIP` [输入]: 板卡IP地址
  - 类型: char*
  - 范围: 有效IP地址字符串
  - 单位: 无
- `ulID` [输入]: 固定标识
  - 类型: unsigned long
  - 范围: 固定为0
  - 单位: 无
- `nRetryTime` [输入]: 重试时间
  - 类型: unsigned short
  - 范围: 固定为0
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 通过IP地址直接打开板卡连接。

**典型应用场景**:
- 简化的IP连接方式
- 动态IP环境

**代码示例**:
```c
int result = 0;

// 通过IP直接连接
// ulID和nRetryTime参数固定为0
result = MC_OpenByIP("192.168.0.200", "192.168.0.1", 0, 0);
if (result != 0) {
    printf("IP连接失败，错误码: %d\n", result);
    return -1;
}

printf("IP连接成功建立\n");
printf("连接方式: 192.168.0.200 -> 192.168.0.1\n");
```

**预期输出**:
```
IP连接成功建立
连接方式: 192.168.0.200 -> 192.168.0.1
```

### MC_Reset

**函数原型**:
```c
int MC_Reset();
```

**参数说明**: 无参数

**返回值**: 参见API返回值说明

**功能描述**: 复位运动控制器，恢复到初始状态。

**典型应用场景**:
- 系统初始化
- 异常状态恢复
- 参数重置

**代码示例**:
```c
int result = 0;

// 打开板卡连接
result = MC_Open(0, "192.168.0.200");
if (result != 0) {
    printf("打开板卡失败，错误码: %d\n", result);
    return -1;
}

// 复位板卡
result = MC_Reset();
if (result != 0) {
    printf("板卡复位失败，错误码: %d\n", result);
    MC_Close();
    return -1;
}

printf("板卡复位成功\n");
```

**预期输出**:
```
板卡复位成功
```

**异常处理机制**:
- 确保板卡已连接
- 复位后需要重新配置参数
- 等待复位完成后再执行其他操作

### MC_Clear

**函数原型**:
```c
int MC_Clear();
```

**参数说明**: 无参数

**返回值**: 参见API返回值说明

**功能描述**: 清除运动控制器内部状态和缓冲区。

**典型应用场景**:
- 清除运动缓冲区
- 重置内部状态标志
- 准备新的运动序列

**代码示例**:
```c
int result = 0;

// 清除控制器状态
result = MC_Clear();
if (result != 0) {
    printf("清除状态失败，错误码: %d\n", result);
    return -1;
}

printf("控制器状态已清除\n");
```

**预期输出**:
```
控制器状态已清除
```

### MC_Close

**函数原型**:
```c
int MC_Close();
```

**参数说明**: 无参数

**返回值**: 参见API返回值说明

**功能描述**: 关闭运动控制器连接，释放资源。

**典型应用场景**:
- 程序退出前清理
- 切换连接方式
- 资源释放

**代码示例**:
```c
int result = 0;

// 完整的板卡操作流程
result = MC_SetCardNo(1);
result += MC_Open(0, "192.168.0.200");
result += MC_Reset();

// 执行运动控制操作
// ...

// 关闭板卡连接
result += MC_Close();

if (result == 0) {
    printf("板卡操作流程完成，连接已关闭\n");
} else {
    printf("板卡操作过程中出现错误\n");
}
```

**预期输出**:
```
板卡操作流程完成，连接已关闭
```

**异常处理机制**:
- 确保所有运动已停止
- 清理内部缓冲区
- 释放网络资源
- 避免资源泄露

---

## 2.2 板卡配置类API

### MC_AlarmOn

**函数原型**:
```c
int MC_AlarmOn(short nAxisNum);
```

**参数说明**:
- `nAxisNum` [输入]: 控制轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置指定轴的驱动报警信号有效，使能轴的报警检测功能。

**典型应用场景**:
- 系统初始化时使能报警检测
- 维护模式下临时启用报警
- 安全保护功能配置

**代码示例**:
```c
int result = 0;

// 打开板卡连接
result = MC_SetCardNo(1);
result += MC_Open(0, "192.168.0.200");
result += MC_Reset();

if (result != 0) {
    printf("板卡初始化失败\n");
    return -1;
}

// 使能轴1-4的报警检测
for (int axis = 1; axis <= 4; axis++) {
    result = MC_AlarmOn(axis);
    if (result != 0) {
        printf("轴%d报警使能失败，错误码: %d\n", axis, result);
    } else {
        printf("轴%d报警检测已使能\n", axis);
    }
}

// 关闭连接
MC_Close();
```

**预期输出**:
```
轴1报警检测已使能
轴2报警检测已使能
轴3报警检测已使能
轴4报警检测已使能
```

**异常处理机制**:
- 检查轴号是否在有效范围内
- 确认板卡已正确初始化
- 验证驱动器连接状态

### MC_AlarmOff

**函数原型**:
```c
int MC_AlarmOff(short nAxisNum);
```

**参数说明**:
- `nAxisNum` [输入]: 控制轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置指定轴的驱动报警信号无效，禁用轴的报警检测功能。

**典型应用场景**:
- 调试时临时禁用报警
- 特殊工况下忽略报警
- 维护模式配置

**代码示例**:
```c
int result = 0;

// 禁用轴1的报警检测（调试用）
result = MC_AlarmOff(1);
if (result == 0) {
    printf("轴1报警检测已禁用（调试模式）\n");
} else {
    printf("禁用轴1报警失败，错误码: %d\n", result);
}

// 注意：生产环境不建议禁用报警检测
printf("警告：报警检测已禁用，请确保操作安全\n");
```

**预期输出**:
```
轴1报警检测已禁用（调试模式）
警告：报警检测已禁用，请确保操作安全
```

**异常处理机制**:
- 仅在调试或维护时使用
- 操作完成后务必重新使能报警
- 记录禁用报警的时间和原因

### MC_AlarmSns

**函数原型**:
```c
int MC_AlarmSns(unsigned short nSense);
```

**参数说明**:
- `nSense` [输入]: 报警信号电平逻辑
  - 类型: unsigned short
  - 范围: 0-65535
  - 单位: 无
  - 位定义: bit0-bit7对应轴1-轴8的电平逻辑

**返回值**: 参见API返回值说明

**功能描述**: 设置运动控制器各轴报警信号的电平逻辑，支持常开/常闭传感器类型。

**电平逻辑位定义**:
- Bit0: 轴1报警电平逻辑 (1=低电平触发, 0=高电平触发)
- Bit1: 轴2报警电平逻辑
- ...
- Bit7: 轴8报警电平逻辑

**典型应用场景**:
- 适配不同类型的报警传感器
- 系统电平逻辑配置
- 传感器类型切换

**代码示例**:
```c
int result = 0;

// 配置轴1-4的报警电平逻辑
// 假设使用常开传感器，低电平触发
unsigned short alarmConfig = 0;

// 轴1: 常开，低电平触发
alarmConfig |= (1 << 0);
// 轴2: 常开，低电平触发
alarmConfig |= (1 << 1);
// 轴3: 常开，低电平触发
alarmConfig |= (1 << 2);
// 轴4: 常开，低电平触发
alarmConfig |= (1 << 3);

result = MC_AlarmSns(alarmConfig);
if (result == 0) {
    printf("报警电平逻辑配置成功\n");
    printf("轴1-4: 常开传感器，低电平触发\n");
} else {
    printf("报警电平逻辑配置失败，错误码: %d\n", result);
}

// 十六进制显示配置值
printf("配置值: 0x%04X\n", alarmConfig);
```

**预期输出**:
```
报警电平逻辑配置成功
轴1-4: 常开传感器，低电平触发
配置值: 0x000F
```

**异常处理机制**:
- 确认传感器类型与配置匹配
- 测试报警信号是否正常工作
- 记录配置参数便于维护

### MC_LmtsOn

**函数原型**:
```c
int MC_LmtsOn(short nAxisNum, short limitType = -1);
```

**参数说明**:
- `nAxisNum` [输入]: 控制轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `limitType` [输入]: 限位类型
  - 类型: short
  - 范围: -1(全部), 0(正限位), 1(负限位)
  - 单位: 无
  - 默认值: -1

**返回值**: 参见API返回值说明

**功能描述**: 设置指定轴的限位信号有效，使能硬限位保护功能。

**限位类型说明**:
- -1: 正限位和负限位都有效
- 0: 仅正限位有效
- 1: 仅负限位有效

**典型应用场景**:
- 系统安全保护配置
- 机械行程限制设置
- 防碰撞保护启用

**代码示例**:
```c
int result = 0;

// 配置轴1的限位保护
// 启用正负限位都有效
result = MC_LmtsOn(1, -1);
if (result == 0) {
    printf("轴1正负限位保护已启用\n");
} else {
    printf("轴1限位保护启用失败，错误码: %d\n", result);
}

// 仅启用轴2的正限位
result = MC_LmtsOn(2, 0);
if (result == 0) {
    printf("轴2正限位保护已启用\n");
}

// 仅启用轴3的负限位
result = MC_LmtsOn(3, 1);
if (result == 0) {
    printf("轴3负限位保护已启用\n");
}

printf("限位保护配置完成\n");
```

**预期输出**:
```
轴1正负限位保护已启用
轴2正限位保护已启用
轴3负限位保护已启用
限位保护配置完成
```

**异常处理机制**:
- 检查限位开关连接状态
- 确认限位信号电平正确
- 测试限位功能是否正常
- 避免在运动中突然启用限位

### MC_LmtsOff

**函数原型**:
```c
int MC_LmtsOff(short nAxisNum, short limitType = -1);
```

**参数说明**:
- `nAxisNum` [输入]: 控制轴号
  - 类型: short
  - 范围: [1, AXIS_MAX_COUNT]
  - 单位: 无
- `limitType` [输入]: 限位类型
  - 类型: short
  - 范围: -1(全部), 0(正限位), 1(负限位)
  - 单位: 无
  - 默认值: -1

**返回值**: 参见API返回值说明

**功能描述**: 设置指定轴的限位信号无效，禁用硬限位保护功能。

**典型应用场景**:
- 调试时临时禁用限位
- 超行程操作需求
- 维护模式配置

**代码示例**:
```c
int result = 0;

// 警告：禁用限位保护存在安全风险
printf("警告：即将禁用限位保护，请确保操作环境安全\n");

// 临时禁用轴1的所有限位保护（调试用）
result = MC_LmtsOff(1, -1);
if (result == 0) {
    printf("轴1限位保护已禁用（调试模式）\n");
} else {
    printf("禁用轴1限位保护失败，错误码: %d\n", result);
}

// 禁用轴2的正限位
result = MC_LmtsOff(2, 0);
if (result == 0) {
    printf("轴2正限位保护已禁用\n");
}

printf("限位保护配置已修改，请注意操作安全\n");
```

**预期输出**:
```
警告：即将禁用限位保护，请确保操作环境安全
轴1限位保护已禁用（调试模式）
轴2正限位保护已禁用
限位保护配置已修改，请注意操作安全
```

**异常处理机制**:
- 仅在特殊情况下禁用限位
- 操作完成后及时重新启用
- 设置软限位作为备份保护
- 记录禁用限位的时间和原因

### MC_LmtSns

**函数原型**:
```c
int MC_LmtSns(unsigned short nSense);
```

**参数说明**:
- `nSense` [输入]: 限位信号电平逻辑
  - 类型: unsigned short
  - 范围: 0-65535
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置运动控制器各轴限位信号的触发电平，适配常开/常闭限位开关。

**位定义说明**:
- Bit0: 轴1正限位逻辑电平 (1=低电平触发, 0=高电平触发)
- Bit1: 轴1负限位逻辑电平 (1=低电平触发, 0=高电平触发)
- Bit2: 轴2正限位逻辑电平
- Bit3: 轴2负限位逻辑电平
- ...
- Bit14: 轴8正限位逻辑电平
- Bit15: 轴8负限位逻辑电平

**典型应用场景**:
- 适配不同类型的限位开关
- 系统电平逻辑统一配置
- 传感器更换后的参数调整

**代码示例**:
```c
int result = 0;

// 配置轴1和轴2的限位电平逻辑
// 假设使用常开限位开关，低电平触发
unsigned short limitConfig = 0;

// 轴1正限位：常开，低电平触发
limitConfig |= (1 << 0);
// 轴1负限位：常开，低电平触发
limitConfig |= (1 << 1);
// 轴2正限位：常开，低电平触发
limitConfig |= (1 << 2);
// 轴2负限位：常开，低电平触发
limitConfig |= (1 << 3);

result = MC_LmtSns(limitConfig);
if (result == 0) {
    printf("限位电平逻辑配置成功\n");
    printf("轴1-2: 常开限位开关，低电平触发\n");
    printf("配置值: 0x%04X\n", limitConfig);

    // 详细显示每个轴的配置
    for (int axis = 1; axis <= 2; axis++) {
        int posBit = (axis - 1) * 2;
        int negBit = posBit + 1;

        bool posLowTrigger = (limitConfig & (1 << posBit)) != 0;
        bool negLowTrigger = (limitConfig & (1 << negBit)) != 0;

        printf("轴%d正限位: %s触发\n", axis, posLowTrigger ? "低电平" : "高电平");
        printf("轴%d负限位: %s触发\n", axis, negLowTrigger ? "低电平" : "高电平");
    }
} else {
    printf("限位电平逻辑配置失败，错误码: %d\n", result);
}
```

**预期输出**:
```
限位电平逻辑配置成功
轴1-2: 常开限位开关，低电平触发
配置值: 0x000F
轴1正限位: 低电平触发
轴1负限位: 低电平触发
轴2正限位: 低电平触发
轴2负限位: 低电平触发
```

**异常处理机制**:
- 确认限位开关类型与配置匹配
- 测试限位信号触发是否正常
- 检查接线是否牢固
- 验证电平逻辑的正确性

### MC_EncOn

**函数原型**:
```c
int MC_EncOn(short nEncoderNum);
```

**参数说明**:
- `nEncoderNum` [输入]: 编码器通道号
  - 类型: short
  - 范围: [1, 16]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置指定编码器通道为"外部编码器"计数方式，使能编码器反馈功能。

**典型应用场景**:
- 闭环控制系统配置
- 位置反馈系统启用
- 精密运动控制应用

**代码示例**:
```c
int result = 0;

// 启用轴1-4的编码器反馈
for (int encoder = 1; encoder <= 4; encoder++) {
    result = MC_EncOn(encoder);
    if (result == 0) {
        printf("编码器%d已启用（外部编码器模式）\n", encoder);
    } else {
        printf("编码器%d启用失败，错误码: %d\n", encoder, result);
    }
}

printf("编码器配置完成\n");
```

**预期输出**:
```
编码器1已启用（外部编码器模式）
编码器2已启用（外部编码器模式）
编码器3已启用（外部编码器模式）
编码器4已启用（外部编码器模式）
编码器配置完成
```

**异常处理机制**:
- 检查编码器连接状态
- 确认编码器供电正常
- 验证编码器信号质量
- 测试编码器计数功能

### MC_EncOff

**函数原型**:
```c
int MC_EncOff(short nEncoderNum);
```

**参数说明**:
- `nEncoderNum` [输入]: 编码器通道号
  - 类型: short
  - 范围: [1, 16]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置指定编码器通道为"脉冲计数器"计数方式，禁用编码器反馈功能。

**典型应用场景**:
- 开环控制系统配置
- 脉冲计数应用
- 编码器故障时的备用方案

**代码示例**:
```c
int result = 0;

// 禁用编码器1（切换到脉冲计数器模式）
result = MC_EncOff(1);
if (result == 0) {
    printf("编码器1已禁用（脉冲计数器模式）\n");
    printf("注意：系统运行在开环模式\n");
} else {
    printf("编码器1禁用失败，错误码: %d\n", result);
}

// 禁用编码器2
result = MC_EncOff(2);
if (result == 0) {
    printf("编码器2已禁用（脉冲计数器模式）\n");
}
```

**预期输出**:
```
编码器1已禁用（脉冲计数器模式）
注意：系统运行在开环模式
编码器2已禁用（脉冲计数器模式）
```

**异常处理机制**:
- 开环模式需要注意安全
- 降低运动速度和加速度
- 增加位置检查频率
- 准备手动急停方案

### MC_EncSns

**函数原型**:
```c
int MC_EncSns(unsigned short nSense);
```

**参数说明**:
- `nSense` [输入]: 编码器计数方向配置
  - 类型: unsigned short
  - 范围: 0-65535
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置编码器的计数方向，支持正向/反向计数配置。

**位定义说明**:
- Bit0-Bit7: 编码器1-8的计数方向 (0=不反向, 1=反向)
- Bit8: 辅助编码器方向 (固定为0)

**典型应用场景**:
- 编码器方向校正
- 机械传动方向调整
- 系统坐标系统一

**代码示例**:
```c
int result = 0;

// 配置编码器方向
// 假设轴2和轴4的编码器方向需要反向
unsigned short encConfig = 0;

// 轴1编码器：正向计数
encConfig |= (0 << 0);
// 轴2编码器：反向计数
encConfig |= (1 << 1);
// 轴3编码器：正向计数
encConfig |= (0 << 2);
// 轴4编码器：反向计数
encConfig |= (1 << 3);

result = MC_EncSns(encConfig);
if (result == 0) {
    printf("编码器方向配置成功\n");

    // 显示配置结果
    for (int axis = 1; axis <= 4; axis++) {
        bool reversed = (encConfig & (1 << (axis - 1))) != 0;
        printf("轴%d编码器: %s计数\n", axis, reversed ? "反向" : "正向");
    }

    printf("配置值: 0x%04X\n", encConfig);
} else {
    printf("编码器方向配置失败，错误码: %d\n", result);
}
```

**预期输出**:
```
编码器方向配置成功
轴1编码器: 正向计数
轴2编码器: 反向计数
轴3编码器: 正向计数
轴4编码器: 反向计数
配置值: 0x000A
```

**异常处理机制**:
- 测试每个轴的运动方向
- 确认方向配置正确性
- 记录方向配置参数
- 定期检查方向一致性

### MC_StepSns

**函数原型**:
```c
int MC_StepSns(unsigned short sense);
```

**参数说明**:
- `sense` [输入]: 脉冲输出方向配置
  - 类型: unsigned short
  - 范围: 0-65535
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置脉冲输出通道的方向，支持脉冲信号方向反转。

**位定义说明**:
- Bit0: 脉冲输出通道1方向 (0=不反向, 1=反向)
- Bit1: 脉冲输出通道2方向
- Bit2: 脉冲输出通道3方向
- Bit3: 脉冲输出通道4方向
- ...

**典型应用场景**:
- 脉冲方向校正
- 驱动器方向配置
- 机械传动方向调整

**代码示例**:
```c
int result = 0;

// 配置脉冲输出方向
// 假设轴2和轴3的脉冲方向需要反转
unsigned short stepConfig = 0;

// 轴1脉冲：不反向
stepConfig |= (0 << 0);
// 轴2脉冲：反向
stepConfig |= (1 << 1);
// 轴3脉冲：反向
stepConfig |= (1 << 2);
// 轴4脉冲：不反向
stepConfig |= (0 << 3);

result = MC_StepSns(stepConfig);
if (result == 0) {
    printf("脉冲方向配置成功\n");

    // 显示配置结果
    for (int axis = 1; axis <= 4; axis++) {
        bool reversed = (stepConfig & (1 << (axis - 1))) != 0;
        printf("轴%d脉冲: %s\n", axis, reversed ? "反向输出" : "正向输出");
    }

    printf("配置值: 0x%04X\n", stepConfig);

    // 建议测试
    printf("建议：执行小范围运动测试方向配置\n");
} else {
    printf("脉冲方向配置失败，错误码: %d\n", result);
}
```

**预期输出**:
```
脉冲方向配置成功
轴1脉冲: 正向输出
轴2脉冲: 反向输出
轴3脉冲: 反向输出
轴4脉冲: 正向输出
配置值: 0x0006
建议：执行小范围运动测试方向配置
```

**异常处理机制**:
- 测试每个轴的实际运动方向
- 确认方向与预期一致
- 调整驱动器方向设置
- 验证限位开关方向逻辑

### MC_HomeSns

**函数原型**:
```c
int MC_HomeSns(unsigned short sense);
```

**参数说明**:
- `sense` [输入]: 原点信号电平逻辑
  - 类型: unsigned short
  - 范围: 0-65535
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置运动控制器HOME输入的电平逻辑，适配常开/常闭原点开关。

**位定义说明**:
- Bit0-Bit7: 轴1-8的HOME电平逻辑 (0=不取反, 1=取反)

**典型应用场景**:
- 原点开关类型适配
- 回零电平逻辑配置
- 系统原点信号统一

**代码示例**:
```c
int result = 0;

// 配置原点信号电平逻辑
// 假设使用常开原点开关，需要电平取反
unsigned short homeConfig = 0;

// 轴1-4：常开开关，电平取反
for (int axis = 1; axis <= 4; axis++) {
    homeConfig |= (1 << (axis - 1));
}

result = MC_HomeSns(homeConfig);
if (result == 0) {
    printf("原点信号电平逻辑配置成功\n");
    printf("配置模式：常开开关，电平取反\n");

    // 显示配置结果
    for (int axis = 1; axis <= 4; axis++) {
        bool inverted = (homeConfig & (1 << (axis - 1))) != 0;
        printf("轴%d原点: 电平%s\n", axis, inverted ? "取反" : "不取反");
    }

    printf("配置值: 0x%04X\n", homeConfig);
} else {
    printf("原点信号配置失败，错误码: %d\n", result);
}
```

**预期输出**:
```
原点信号电平逻辑配置成功
配置模式：常开开关，电平取反
轴1原点: 电平取反
轴2原点: 电平取反
轴3原点: 电平取反
轴4原点: 电平取反
配置值: 0x000F
```

**异常处理机制**:
- 测试原点信号触发状态
- 确认原点开关工作正常
- 验证回零功能正确性
- 检查原点信号稳定性

---

