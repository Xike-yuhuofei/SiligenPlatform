# MC_CmpPluse 函数教程

## 函数原型

```c
int MC_CmpPluse(short nChannelMask, short nPluseType1, short nPluseType2,
                short nTime1, short nTime2, short nTimeFlag1, short nTimeFlag2)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nChannelMask | short | 通道掩码，bit0表示通道1，bit1表示通道2 |
| nPluseType1 | short | 通道1输出类型，0：低电平，1：高电平，2：脉冲 |
| nPluseType2 | short | 通道2输出类型，0：低电平，1：高电平，2：脉冲 |
| nTime1 | short | 通道1脉冲持续时间，输出类型为脉冲时该参数有效 |
| nTime2 | short | 通道2脉冲持续时间，输出类型为脉冲时该参数有效 |
| nTimeFlag1 | short | 比较器1的脉冲时间单位，0：μs，1：ms |
| nTimeFlag2 | short | 比较器2的脉冲时间单位，0：μs，1：ms |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：强制输出高电平
```cpp
// 从DemoVCDlg.cpp中提取的强制高电平输出代码
void CDemoVCDlg::OnBnClickedComboCmpH()
{
    // 强制比较器1输出一个高电平（调试硬件用）
    g_MultiCard.MC_CmpPluse(1,1,1,0,0,0,0);
}
```

### 示例2：强制输出低电平
```cpp
// 从DemoVCDlg.cpp中提取的强制低电平输出代码
void CDemoVCDlg::OnBnClickedComboCmpL()
{
    // 强制比较器1输出一个低电平（调试硬件用）
    g_MultiCard.MC_CmpPluse(1,0,0,0,0,0,0);
}
```

### 示例3：输出单个脉冲
```cpp
// 从DemoVCDlg.cpp中提取的脉冲输出代码
void CDemoVCDlg::OnBnClickedComboCmpP()
{
    // 强制比较器1输出一个脉冲（调试硬件用）
    g_MultiCard.MC_CmpPluse(1,2,0,1000,0,m_ComboBoxCMPUnit.GetCurSel(),0);
}
```

### 示例4：双通道同时控制
```cpp
// 双通道同时输出不同信号
void ControlBothChannels()
{
    int iRes = 0;

    // 通道1输出高电平，通道2输出脉冲
    iRes = g_MultiCard.MC_CmpPluse(0x03, 1, 2, 0, 500, 0, 1);

    if(iRes == 0)
    {
        TRACE("双通道控制成功：通道1高电平，通道2脉冲\n");
    }
    else
    {
        TRACE("双通道控制失败，错误代码：%d\n", iRes);
    }
}
```

### 示例5：可配置脉冲输出
```cpp
// 根据用户配置输出脉冲
void OutputConfigurablePulse(short channel, short pulseTime, short timeUnit)
{
    // 构建通道掩码
    short channelMask = 0x01 << (channel - 1);

    // 输出单个脉冲
    int iRes = g_MultiCard.MC_CmpPluse(channelMask, 2, 0, pulseTime, 0, timeUnit, 0);

    if(iRes == 0)
    {
        const char* unitStr = (timeUnit == 0) ? "μs" : "ms";
        TRACE("通道%d输出脉冲成功，时间：%d %s\n", channel, pulseTime, unitStr);
    }
    else
    {
        TRACE("通道%d脉冲输出失败，错误代码：%d\n", channel, iRes);
    }
}
```

## 参数映射表

| 应用场景 | nChannelMask | nPluseType1 | nPluseType2 | nTime1 | nTime2 | nTimeFlag1 | 说明 |
|----------|--------------|-------------|-------------|--------|--------|------------|------|
| 通道1高电平 | 0x01 | 1 | 0 | 0 | 0 | 0 | 通道1输出高电平 |
| 通道1低电平 | 0x01 | 0 | 0 | 0 | 0 | 0 | 通道1输出低电平 |
| 通道1脉冲 | 0x01 | 2 | 0 | 1000 | 0 | 0 | 通道1输出1ms脉冲 |
| 通道2脉冲 | 0x02 | 0 | 2 | 500 | 0 | 1 | 通道2输出500μs脉冲 |
| 双通道控制 | 0x03 | 1 | 2 | 0 | 1000 | 0 | 通道1高电平，通道2脉冲 |
| 微秒精度 | 通道掩码 | 2 | 0 | 100 | 0 | 0 | 100μs精密脉冲 |

## 关键说明

1. **输出类型选择**：
   - **低电平(0)**：强制输出低电平信号
   - **高电平(1)**：强制输出高电平信号
   - **脉冲(2)**：输出指定宽度的脉冲信号

2. **通道控制**：
   - **通道1**：对应bit0，掩码值为0x01
   - **通道2**：对应bit1，掩码值为0x02
   - **双通道**：同时控制两个通道，掩码值为0x03

3. **脉冲时间设置**：
   - **nTime参数**：仅在输出类型为脉冲时有效
   - **nTimeFlag控制单位**：0表示微秒，1表示毫秒
   - **时间精度**：支持微秒级精确控制

4. **立即输出特性**：
   - 函数调用后立即输出信号
   - 不依赖运动控制状态
   - 适合硬件调试和手动控制

5. **应用场景**：
   - **硬件调试**：手动测试输出信号
   - **状态指示**：输出状态信号给外部设备
   - **触发控制**：触发外部设备动作
   - **同步信号**：提供外部同步信号

6. **调试用途**：
   - 验证硬件连接
   - 测试外部设备响应
   - 手动控制系统状态
   - 信号质量检查

7. **注意事项**：
   - 脉冲输出时注意时间参数设置
   - 避免过于频繁的脉冲输出
   - 注意输出信号的电气特性
   - 考虑外部设备的输入要求

## 函数区别

- **MC_CmpPluse() vs MC_CmpBufData()**: MC_CmpPluse()立即输出信号，MC_CmpBufData()设置位置触发输出
- **MC_CmpPluse() vs MC_CmpBufSts()**: MC_CmpPluse()控制输出，MC_CmpBufSts()读取状态
- **立即 vs 触发**: MC_CmpPluse()是即时输出，MC_CmpBufData()是位置触发输出

---

**使用建议**: MC_CmpPluse()主要用于硬件调试和手动控制。在生产环境中，建议使用MC_CmpBufData()进行位置触发控制，这样可以实现更精确的同步控制。调试时可以通过设置不同的输出类型和时间参数来验证硬件连接和外部设备的功能。