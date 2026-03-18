## 2.3 IO/模拟量/PWM操作API

### MC_GetDiRaw

**函数原型**:
```c
int MC_GetDiRaw(short nDiType, long *pValue);
```

**参数说明**:
- `nDiType` [输入]: 数字IO类型
  - 类型: short
  - 范围: 0(正限位), 1(负限位), 2(驱动报警), 3(原点开关), 4(通用输入), 7(手轮IO输入)
  - 单位: 无
- `pValue` [输出]: IO输入值存放指针
  - 类型: long*
  - 范围: 0-4294967295
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 获取IO输入原始值，包含主卡IO、限位、零位等信号。

**典型应用场景**:
- 系统状态监控
- 传感器信号读取
- 安全信号检测

**代码示例**:
```c
int result = 0;
long diValue = 0;

// 读取通用输入状态
result = MC_GetDiRaw(4, &diValue);
if (result == 0) {
    printf("通用输入状态: 0x%08lX\n", diValue);

    // 按位显示输入状态
    for (int i = 0; i < 16; i++) {
        int state = (diValue >> i) & 1;
        printf("X%d: %s\n", i, state ? "ON" : "OFF");
    }
} else {
    printf("读取通用输入失败，错误码: %d\n", result);
}

// 读取原点开关状态
result = MC_GetDiRaw(3, &diValue);
if (result == 0) {
    printf("原点开关状态: 0x%02lX\n", diValue);

    for (int i = 0; i < 8; i++) {
        int state = (diValue >> i) & 1;
        if (state) {
            printf("轴%d已触发原点信号\n", i + 1);
        }
    }
}
```

**预期输出**:
```
通用输入状态: 0x0000A5A5
X0: ON
X1: OFF
X2: ON
X3: OFF
...
原点开关状态: 0x05
轴1已触发原点信号
轴3已触发原点信号
```

**异常处理机制**:
- 检查指针参数是否为NULL
- 确认IO类型参数正确
- 验证硬件连接状态

### MC_SetExtDoValue

**函数原型**:
```c
int MC_SetExtDoValue(short nCardIndex, unsigned long *value, short nCount = 1);
```

**参数说明**:
- `nCardIndex` [输入]: 起始板卡索引
  - 类型: short
  - 范围: 0(主模块), 1-63(扩展模块)
  - 单位: 无
- `value` [输入]: IO输出值数组指针
  - 类型: unsigned long*
  - 范围: 0-4294967295
  - 单位: 无
- `nCount` [输入]: 本次设置的模块数量
  - 类型: short
  - 范围: 1-64
  - 单位: 无
  - 默认值: 1

**返回值**: 参见API返回值说明

**功能描述**: 设置IO输出值，支持主模块和扩展模块的批量配置。

**典型应用场景**:
- 批量IO输出控制
- 执行器状态设置
- 信号指示灯控制

**代码示例**:
```c
int result = 0;
unsigned long outputValues[3];

// 配置3个模块的输出值
outputValues[0] = 0x0000FFFF;  // 主模块：Y0-Y15输出
outputValues[1] = 0x0000000F;  // 扩展模块1：Y0-Y3输出
outputValues[2] = 0x00000000;  // 扩展模块2：全部关闭

// 批量设置输出
result = MC_SetExtDoValue(0, outputValues, 3);
if (result == 0) {
    printf("IO输出批量设置成功\n");
    printf("主模块输出: 0x%08lX\n", outputValues[0]);
    printf("扩展模块1输出: 0x%08lX\n", outputValues[1]);
    printf("扩展模块2输出: 0x%08lX\n", outputValues[2]);

    // 显示具体输出状态
    printf("主模块Y0-Y15状态:\n");
    for (int i = 0; i < 16; i++) {
        int state = (outputValues[0] >> i) & 1;
        printf("Y%d: %s ", i, state ? "ON" : "OFF");
        if ((i + 1) % 8 == 0) printf("\n");
    }
} else {
    printf("IO输出设置失败，错误码: %d\n", result);
}
```

**预期输出**:
```
IO输出批量设置成功
主模块输出: 0x0000FFFF
扩展模块1输出: 0x0000000F
扩展模块2输出: 0x00000000
主模块Y0-Y15状态:
Y0: ON Y1: ON Y2: ON Y3: ON Y4: ON Y5: ON Y6: ON Y7: ON
Y8: ON Y9: ON Y10: ON Y11: ON Y12: ON Y13: ON Y14: ON Y15: ON
```

**异常处理机制**:
- 检查数组指针有效性
- 确认模块索引范围正确
- 验证模块数量合理性

### MC_SetExtDoBit

**函数原型**:
```c
int MC_SetExtDoBit(short nCardIndex, short nBitIndex, unsigned short nValue);
```

**参数说明**:
- `nCardIndex` [输入]: 板卡索引
  - 类型: short
  - 范围: 0(主卡), 1-63(扩展卡)
  - 单位: 无
- `nBitIndex` [输入]: IO位索引号
  - 类型: short
  - 范围: 0-31
  - 单位: 无
- `nValue` [输入]: IO输出值
  - 类型: unsigned short
  - 范围: 0(关闭), 1(打开)
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置指定IO模块的指定位输出，支持精确的单点控制。

**典型应用场景**:
- 单个执行器控制
- 指示灯状态切换
- 精确信号输出

**代码示例**:
```c
int result = 0;

// 控制主卡的不同IO位
printf("开始IO位控制测试...\n");

// 设置Y0为高电平
result = MC_SetExtDoBit(0, 0, 1);
if (result == 0) {
    printf("Y0设置为ON\n");
}

// 设置Y1为低电平
result = MC_SetExtDoBit(0, 1, 0);
if (result == 0) {
    printf("Y1设置为OFF\n");
}

// 设置扩展卡1的Y0为高电平
result = MC_SetExtDoBit(1, 0, 1);
if (result == 0) {
    printf("扩展卡1 Y0设置为ON\n");
}

// 批量设置主卡Y8-Y15
for (int i = 8; i <= 15; i++) {
    result = MC_SetExtDoBit(0, i, (i % 2) ? 1 : 0);
    if (result == 0) {
        printf("Y%d设置为%s\n", i, (i % 2) ? "ON" : "OFF");
    }
}

printf("IO位控制测试完成\n");
```

**预期输出**:
```
开始IO位控制测试...
Y0设置为ON
Y1设置为OFF
扩展卡1 Y0设置为ON
Y8设置为OFF
Y9设置为ON
Y10设置为OFF
Y11设置为ON
Y12设置为OFF
Y13设置为ON
Y14设置为OFF
Y15设置为ON
IO位控制测试完成
```

**异常处理机制**:
- 检查位索引范围
- 确认板卡索引有效
- 验证输出值合法性

### MC_GetExtDiBit

**函数原型**:
```c
int MC_GetExtDiBit(short nCardIndex, short nBitIndex, unsigned short *pValue);
```

**参数说明**:
- `nCardIndex` [输入]: 板卡索引
  - 类型: short
  - 范围: 0(主卡), 1-63(扩展卡)
  - 单位: 无
- `nBitIndex` [输入]: IO位索引号
  - 类型: short
  - 范围: 0-31
  - 单位: 无
- `pValue` [输出]: IO输入值存放指针
  - 类型: unsigned short*
  - 范围: 0-1
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 获取指定IO模块的指定位输入状态，实现精确的单点输入检测。

**典型应用场景**:
- 传感器状态检测
- 按键输入读取
- 安全信号监控

**代码示例**:
```c
int result = 0;
unsigned short inputValue = 0;

// 监控主卡的输入位
printf("开始IO输入状态监控...\n");

// 检测X0状态
result = MC_GetExtDiBit(0, 0, &inputValue);
if (result == 0) {
    printf("X0状态: %s\n", inputValue ? "ON" : "OFF");
}

// 批量检测主卡X0-X7状态
printf("主卡X0-X7状态:\n");
for (int i = 0; i < 8; i++) {
    result = MC_GetExtDiBit(0, i, &inputValue);
    if (result == 0) {
        printf("X%d: %s ", i, inputValue ? "ON" : "OFF");
        if ((i + 1) % 4 == 0) printf("\n");
    }
}

// 检测扩展卡1的输入
printf("\n扩展卡1输入状态:\n");
for (int i = 0; i < 4; i++) {
    result = MC_GetExtDiBit(1, i, &inputValue);
    if (result == 0) {
        printf("扩展卡1 X%d: %s\n", i, inputValue ? "ON" : "OFF");
    }
}
```

**预期输出**:
```
开始IO输入状态监控...
X0状态: ON
主卡X0-X7状态:
X0: ON X1: OFF X2: ON X3: OFF
X4: OFF X5: ON X6: OFF X7: ON

扩展卡1输入状态:
扩展卡1 X0: OFF
扩展卡1 X1: ON
扩展卡1 X2: OFF
扩展卡1 X3: ON
```

**异常处理机制**:
- 检查指针参数有效性
- 确认位索引和卡号范围
- 验证硬件连接状态

### MC_SetDac (模拟量版本专用)

**函数原型**:
```c
int MC_SetDac(short nDacNum, double dValue);
```

**参数说明**:
- `nDacNum` [输入]: DAC通道号
  - 类型: short
  - 范围: [1, 4]
  - 单位: 无
- `dValue` [输入]: DAC输出电压值
  - 类型: double
  - 范围: [-10, 10]
  - 单位: V

**返回值**: 参见API返回值说明

**功能描述**: 设置DAC输出电压，仅适用于模拟量版本的控制卡。

**典型应用场景**:
- 模拟量信号输出
- 速度控制信号
- 压力/流量调节

**代码示例**:
```c
int result = 0;

// DAC输出电压测试
printf("开始DAC输出测试...\n");

// 设置DAC1输出2.5V
result = MC_SetDac(1, 2.5);
if (result == 0) {
    printf("DAC1输出: %.2fV\n", 2.5);
} else {
    printf("DAC1设置失败，错误码: %d\n", result);
}

// 设置DAC2输出-3.5V
result = MC_SetDac(2, -3.5);
if (result == 0) {
    printf("DAC2输出: %.2fV\n", -3.5);
}

// 设置DAC3输出0V
result = MC_SetDac(3, 0.0);
if (result == 0) {
    printf("DAC3输出: %.2fV\n", 0.0);
}

// 测试边界值
printf("\n测试边界值:\n");
result = MC_SetDac(4, 10.0);
if (result == 0) {
    printf("DAC4输出: %.2fV (最大值)\n", 10.0);
}

result = MC_SetDac(1, -10.0);
if (result == 0) {
    printf("DAC1输出: %.2fV (最小值)\n", -10.0);
}

printf("DAC输出测试完成\n");
```

**预期输出**:
```
开始DAC输出测试...
DAC1输出: 2.50V
DAC2输出: -3.50V
DAC3输出: 0.00V

测试边界值:
DAC4输出: 10.00V (最大值)
DAC1输出: -10.00V (最小值)
DAC输出测试完成
```

**异常处理机制**:
- 检查通道号范围
- 验证电压值范围
- 确认硬件为模拟量版本

### MC_GetAdc (模拟量版本专用)

**函数原型**:
```c
int MC_GetAdc(short nAdcNum, double *dValue);
```

**参数说明**:
- `nAdcNum` [输入]: ADC通道号
  - 类型: short
  - 范围: [1, 16]
  - 单位: 无
- `dValue` [输出]: ADC输入电压值存放指针
  - 类型: double*
  - 范围: [-10, 10]
  - 单位: V

**返回值**: 参见API返回值说明

**功能描述**: 读取ADC输入电压，仅适用于模拟量版本的控制卡。

**典型应用场景**:
- 模拟量传感器采集
- 电压监控
- 电流/压力检测

**代码示例**:
```c
int result = 0;
double voltage = 0.0;

// ADC输入电压采集
printf("开始ADC输入采集...\n");

// 读取前8个ADC通道
for (int channel = 1; channel <= 8; channel++) {
    result = MC_GetAdc(channel, &voltage);
    if (result == 0) {
        printf("ADC%d: %.3fV", channel, voltage);

        // 判断电压状态
        if (voltage > 8.0) {
            printf(" (高电平)");
        } else if (voltage < 2.0) {
            printf(" (低电平)");
        } else {
            printf(" (中间值)");
        }
        printf("\n");
    } else {
        printf("ADC%d读取失败，错误码: %d\n", channel, result);
    }
}

// 连续采集ADC1，用于监控变化
printf("\n连续监控ADC1 (5次):\n");
for (int i = 0; i < 5; i++) {
    result = MC_GetAdc(1, &voltage);
    if (result == 0) {
        printf("第%d次: %.3fV\n", i + 1, voltage);
    }
    // 延时100ms
    Sleep(100);
}
```

**预期输出**:
```
开始ADC输入采集...
ADC1: 3.245V (中间值)
ADC2: 9.876V (高电平)
ADC3: 0.123V (低电平)
ADC4: 5.000V (中间值)
ADC5: 1.567V (低电平)
ADC6: 7.890V (中间值)
ADC7: 2.345V (低电平)
ADC8: 4.567V (中间值)

连续监控ADC1 (5次):
第1次: 3.245V
第2次: 3.247V
第3次: 3.246V
第4次: 3.248V
第5次: 3.245V
```

**异常处理机制**:
- 检查指针参数有效性
- 确认通道号范围
- 验证硬件连接状态

### MC_SetPwm (模拟量版本专用)

**函数原型**:
```c
int MC_SetPwm(short nPwmNum, double dFreq, double dDuty);
```

**参数说明**:
- `nPwmNum` [输入]: PWM通道号
  - 类型: short
  - 范围: [1, 4]
  - 单位: 无
- `dFreq` [输入]: PWM输出频率
  - 类型: double
  - 范围: [1, 100000]
  - 单位: Hz
- `dDuty` [输入]: PWM输出占空比
  - 类型: double
  - 范围: [0, 1]
  - 单位: 无

**返回值**: 参见API返回值说明

**功能描述**: 设置PWM输出频率及占空比，仅适用于模拟量版本的控制卡。

**典型应用场景**:
- 电机调速控制
- 阀门开度调节
- LED亮度控制
- 加热器功率控制

**代码示例**:
```c
int result = 0;

// PWM输出配置测试
printf("开始PWM输出配置测试...\n");

// 配置PWM1：1kHz频率，50%占空比
result = MC_SetPwm(1, 1000.0, 0.5);
if (result == 0) {
    printf("PWM1: 频率=%.0fHz, 占空比=%.1f%%\n", 1000.0, 0.5 * 100);
}

// 配置PWM2：10kHz频率，25%占空比
result = MC_SetPwm(2, 10000.0, 0.25);
if (result == 0) {
    printf("PWM2: 频率=%.0fHz, 占空比=%.1f%%\n", 10000.0, 0.25 * 100);
}

// 配置PWM3：500Hz频率，75%占空比
result = MC_SetPwm(3, 500.0, 0.75);
if (result == 0) {
    printf("PWM3: 频率=%.0fHz, 占空比=%.1f%%\n", 500.0, 0.75 * 100);
}

// 测试不同占空比
printf("\nPWM4占空比测试:\n");
double duties[] = {0.0, 0.1, 0.3, 0.5, 0.7, 0.9, 1.0};
for (int i = 0; i < 7; i++) {
    result = MC_SetPwm(4, 2000.0, duties[i]);
    if (result == 0) {
        printf("占空比%.1f%%: 设置成功\n", duties[i] * 100);
    }
    Sleep(200); // 延时200ms观察效果
}

printf("PWM配置测试完成\n");
```

**预期输出**:
```
开始PWM输出配置测试...
PWM1: 频率=1000Hz, 占空比=50.0%
PWM2: 频率=10000Hz, 占空比=25.0%
PWM3: 频率=500Hz, 占空比=75.0%

PWM4占空比测试:
占空比0.0%: 设置成功
占空比10.0%: 设置成功
占空比30.0%: 设置成功
占空比50.0%: 设置成功
占空比70.0%: 设置成功
占空比90.0%: 设置成功
占空比100.0%: 设置成功
PWM配置测试完成
```

**异常处理机制**:
- 检查通道号范围
- 验证频率和占空比范围
- 确认硬件为模拟量版本

### MC_SetDoBitReverse

**函数原型**:
```c
int MC_SetDoBitReverse(short nCardNum, short nDoIndex, short nValue, short nReverseTime);
```

**参数说明**:
- `nCardNum` [输入]: 卡号
  - 类型: short
  - 范围: 0(主卡), 1-4(扩展IO卡)
  - 单位: 无
- `nDoIndex` [输入]: IO索引
  - 类型: short
  - 范围: [1, 16]
  - 单位: 无
- `nValue` [输入]: 设置数字IO输出电平
  - 类型: short
  - 范围: 0(低电平), 1(高电平)
  - 单位: 无
- `nReverseTime` [输入]: 维持电平的时间
  - 类型: short
  - 范围: [0, 32767]
  - 单位: ms

**返回值**: 参见API返回值说明

**功能描述**: 设置数字IO输出指定时间的单个脉冲，用于控制需要定时切换的设备。

**典型应用场景**:
- 电磁阀控制
- 蜂鸣器鸣叫
- 指示灯闪烁
- 步进电机脉冲

**代码示例**:
```c
int result = 0;

// 单脉冲输出测试
printf("开始单脉冲输出测试...\n");

// 在Y3输出100ms的高电平脉冲
result = MC_SetDoBitReverse(0, 3, 1, 100);
if (result == 0) {
    printf("Y3输出100ms高电平脉冲\n");
}

// 在Y4输出200ms的低电平脉冲
result = MC_SetDoBitReverse(0, 4, 0, 200);
if (result == 0) {
    printf("Y4输出200ms低电平脉冲\n");
}

// 批量脉冲测试
printf("\n批量脉冲测试 (Y5-Y7):\n");
for (int i = 5; i <= 7; i++) {
    int pulseTime = 50 * i; // 50ms, 100ms, 150ms
    result = MC_SetDoBitReverse(0, i, 1, pulseTime);
    if (result == 0) {
        printf("Y%d输出%dms高电平脉冲\n", i, pulseTime);
    }
    Sleep(pulseTime + 50); // 等待脉冲完成
}

// 蜂鸣器测试（假设Y8连接蜂鸣器）
printf("\n蜂鸣器测试:\n");
for (int i = 0; i < 3; i++) {
    result = MC_SetDoBitReverse(0, 8, 1, 200); // 200ms鸣叫
    if (result == 0) {
        printf("第%d次鸣叫\n", i + 1);
    }
    Sleep(300); // 间隔100ms
}

printf("单脉冲输出测试完成\n");
```

**预期输出**:
```
开始单脉冲输出测试...
Y3输出100ms高电平脉冲
Y4输出200ms低电平脉冲

批量脉冲测试 (Y5-Y7):
Y5输出50ms高电平脉冲
Y6输出100ms高电平脉冲
Y7输出150ms高电平脉冲

蜂鸣器测试:
第1次鸣叫
第2次鸣叫
第3次鸣叫
单脉冲输出测试完成
```

**异常处理机制**:
- 检查卡号和IO索引范围
- 验证脉冲时间合理性
- 确认硬件连接状态

---

