# MC_BufIOReverse 函数教程

## 函数原型

```c
int MC_BufIOReverse(short nCrdNum, unsigned short nCardIndex, unsigned short nIOPortIndex, unsigned short nMask, unsigned short nData, unsigned long lTimeUS, short nFifoIndex, long segNum)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| nCardIndex | unsigned short | 卡索引 |
| nIOPortIndex | unsigned short | IO端口索引 |
| nMask | unsigned short | IO掩码，指定要操作的位 |
| nData | unsigned short | IO数据，初始值 |
| lTimeUS | unsigned long | 反向时间（微秒） |
| nFifoIndex | short | FIFO索引，通常为0 |
| segNum | long | 用户自定义段号 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本反向IO控制
```cpp
// 从DemoVCDlg.cpp中提取的反向IO控制代码
void CDemoVCDlg::OnBnClickedButton21()
{
    int iRes = 0;

    // 设置反向IO控制：端口12，初始值0x80，持续时间5000微秒
    iRes = g_MultiCard.MC_BufIOReverse(1,12,0,0X80,0X80,5000,0,0);

    if(iRes == 0)
    {
        TRACE("反向IO控制设置成功\n");
        TRACE("端口12，初始值0x80，反向时间5000微秒\n");
    }
    else
    {
        TRACE("反向IO控制设置失败，错误代码: %d\n", iRes);
    }
}
```

### 示例2：脉冲输出控制
```cpp
// 从DemoVCDlg.cpp中提取的脉冲输出控制代码片段
void PulseControlExample()
{
    // 生成精确脉冲信号：高电平5ms，低电平5ms
    g_MultiCard.MC_BufIOReverse(1,12,0,0X80,0X80,5000,0,0);

    // 延时后可以设置下一个脉冲
    g_MultiCard.MC_BufDelay(1,10000,0,1);

    // 再次生成脉冲
    g_MultiCard.MC_BufIOReverse(1,12,0,0X80,0X80,5000,0,0);

    TRACE("脉冲控制完成\n");
}
```

## 参数映射表

| 应用场景 | lTimeUS | nData | 说明 |
|----------|---------|-------|------|
| 短脉冲 | 100-1000 | 0x80/0x00 | 精确定时脉冲 |
| 中脉冲 | 1000-10000 | 0x80/0x00 | 常规控制脉冲 |
| 长脉冲 | 10000-50000 | 0x80/0x00 | 长时间信号 |
| 周期信号 | 自定义 | 0x80/0x00 | 周期性信号 |

## 关键说明

1. **反向机制**：
   - IO输出在指定时间后自动反向
   - 初始输出nData值
   - 经过lTimeUS时间后自动输出~nData值

2. **时间精度**：
   - 时间单位为微秒（μs）
   - 支持高精度定时控制
   - 最小时间分辨率取决于硬件性能

3. **掩码控制**：
   - nMask指定要反向的位
   - 只有掩码指定的位会反向
   - 其他位保持不变

4. **缓冲区特性**：
   - 反向IO指令进入缓冲区队列
   - 与运动指令同步执行
   - 支持精确的时序控制

5. **应用场景**：
   - 脉冲控制（激光、阀门等）
   - 精确定时开关控制
   - 周期信号生成
   - 触发信号控制

6. **注意事项**：
   - 确认端口硬件支持高速开关
   - 考虑信号上升沿和下降沿时间
   - 避免过于频繁的开关操作

## 函数区别

- **MC_BufIOReverse() vs MC_BufIO()**: 自动反向 vs 手动控制
- **MC_BufIOReverse() vs MC_BufDelay()**: IO反向 vs 延时等待
- **精确定时 vs 普通控制**: 微秒级精度 vs 毫秒级精度

---

**使用建议**: 反向IO控制适用于需要精确定时脉冲的应用场景，如激光控制、阀门开关、触发信号等。时间参数应根据实际需求精确计算，确保控制效果。对于高速开关应用，需要确认硬件IO端口的响应速度支持。建议先进行小范围测试，验证时间精度和信号质量后再投入使用。