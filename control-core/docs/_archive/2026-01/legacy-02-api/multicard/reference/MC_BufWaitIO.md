# MC_BufWaitIO 函数教程

## 函数原型

```c
int MC_BufWaitIO(short nCrdNum, unsigned short nCardIndex, unsigned short nIOPortIndex, unsigned short nLevel, unsigned short nMask, unsigned long lWaitTimeMS, unsigned short nFilterTime, short nFifoIndex, long segNum)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| nCardIndex | unsigned short | 卡索引 |
| nIOPortIndex | unsigned short | IO端口索引 |
| nLevel | unsigned short | 等待电平（0=低电平，1=高电平） |
| nMask | unsigned short | IO掩码，指定要检测的位 |
| lWaitTimeMS | unsigned long | 等待超时时间（毫秒），0表示无限等待 |
| nFilterTime | unsigned short | 滤波时间（毫秒） |
| nFifoIndex | short | FIFO索引，通常为0 |
| segNum | long | 用户自定义段号 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本IO等待
```cpp
// 从DemoVCDlg.cpp中提取的IO等待代码注释
/*
// 等待IO，nLevel=1等待有信号输入，lWaitTimeMS=0一直等待
g_MultiCard.MC_BufWaitIO(1,0,0,1,0,100,0,3);
*/
```

### 示例2：IO等待与运动结合
```cpp
// 从DemoVCDlg.cpp中提取的IO等待与运动结合代码
void IOWaitMotionExample()
{
    int iRes = 0;

    // 等待IO信号（高电平）
    // 等待端口0，高电平，掩码0，超时100ms，滤波时间0
    g_MultiCard.MC_BufWaitIO(1,0,0,1,0,100,0,3);

    // 继续执行运动
    iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);

    // 延时2000ms
    iRes = g_MultiCard.MC_BufDelay(1,2000,0,8);

    // 再次运动
    iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);

    // 插入最后一行标识符
    iRes += g_MultiCard.MC_CrdData(1,NULL,0);

    TRACE("IO等待与运动结合完成\n");
}
```

## 参数映射表

| 应用场景 | nLevel | lWaitTimeMS | nFilterTime | 说明 |
|----------|--------|-------------|-------------|------|
| 等待高电平 | 1 | 0 | 0 | 无限等待高电平 |
| 等待低电平 | 0 | 1000 | 10 | 等待低电平，超时1秒 |
| 脉冲检测 | 1 | 5000 | 50 | 等待脉冲，超时5秒 |
| 信号稳定 | 0/1 | 2000 | 100 | 等待信号稳定 |

## 关键说明

1. **等待机制**：
   - 等待指定IO端口达到指定电平
   - 支持超时机制防止无限等待
   - 滤波时间消除信号抖动

2. **电平检测**：
   - nLevel=1：等待高电平信号
   - nLevel=0：等待低电平信号
   - 通过nMask指定要检测的位

3. **超时控制**：
   - lWaitTimeMS=0：无限等待
   - lWaitTimeMS>0：超时时间（毫秒）
   - 超时后继续执行后续指令

4. **滤波功能**：
   - nFilterTime为信号滤波时间
   - 消除开关抖动和噪声干扰
   - 确保信号检测的稳定性

5. **缓冲区特性**：
   - IO等待指令进入缓冲区队列
   - 与运动指令同步执行
   - 保证时序的正确性

6. **应用场景**：
   - 等待传感器信号
   - 检测限位开关状态
   - 同步外部设备
   - 工艺流程控制

7. **注意事项**：
   - 合理设置超时时间避免死等
   - 滤波时间不宜过长影响响应速度
   - 确认IO端口硬件连接正确

## 函数区别

- **MC_BufWaitIO() vs MC_BufDelay()**: 条件等待 vs 时间等待
- **MC_BufWaitIO() vs MC_BufIO()**: 等待输入 vs 控制输出
- **无限等待 vs 超时等待**: 无时间限制 vs 有时间限制

---

**使用建议**: IO等待指令适用于需要与外部设备同步的场景，如等待传感器信号、检测工件到位等。建议设置合理的超时时间，避免因外部设备故障导致系统无限等待。滤波时间应根据实际信号质量设置，过长的滤波时间会影响响应速度，过短则可能无法有效消除信号抖动。在使用前应确认IO端口硬件连接和信号电平的正确性。