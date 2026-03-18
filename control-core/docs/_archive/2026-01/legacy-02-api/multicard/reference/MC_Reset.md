# MC_Reset 函数教程

## 函数原型

```c
int MC_Reset()
```

## 参数说明

此函数无需参数。

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：连接成功后复位
```cpp
// 在MC_Open成功后立即复位控制卡
int iRes = 0;
iRes = g_MultiCard.MC_Open(1,"192.168.0.200",0,"192.168.0.1",0);

if(iRes)
{
    MessageBox("Open Card Fail,Please turn off wifi ,check PC IP address or connection!");
}
else
{
    g_MultiCard.MC_Reset();  // 复位控制卡到初始状态
    MessageBox("Open Card Successful!");
    OnCbnSelchangeCombo1();  // 初始化界面状态
}
```

### 示例2：手动复位控制卡
```cpp
// 通过按钮手动复位控制卡
void CDemoVCDlg::OnBnClickedButton8()
{
    g_MultiCard.MC_Reset();
}
```

## 参数映射表

| 使用场景 | 调用时机 | 说明 |
|----------|----------|------|
| 初始化复位 | MC_Open()成功后 | 确保控制卡处于已知状态 |
| 手动复位 | 用户点击复位按钮 | 处理异常或重新开始 |
| 错误恢复 | 运动异常停止后 | 清除错误状态，重新开始 |

## 关键说明

1. **复位效果**：
   - 清除所有轴的运动状态
   - 重置坐标系参数
   - 清空运动缓冲区
   - 将所有参数恢复到默认值

2. **使用时机**：
   - **初始连接后**：建议在MC_Open()成功后立即调用
   - **错误恢复**：当运动控制出现异常时使用
   - **重新开始**：需要清空当前状态重新开始时

3. **注意事项**：
   - 复位后需要重新配置所有参数（坐标系、运动模式等）
   - 轴使能状态会被清除，需要重新使能
   - 原点位置信息会丢失，需要重新执行回零操作

4. **与相关函数的关系**：
   - MC_Reset()是硬件复位，比MC_Stop()更彻底
   - 复位后需要重新调用MC_SetCrdPrm()等配置函数
   - 建议在复杂操作前先复位确保系统状态一致

## 函数区别

- **MC_Reset() vs MC_Stop()**: MC_Reset()是完整硬件复位，清除所有状态；MC_Stop()只是停止运动，保留配置
- **MC_Reset() vs MC_ClrSts()**: MC_Reset()复位整个控制卡；MC_ClrSts()只清除单个轴的状态
- **MC_Reset() vs MC_Close()**: MC_Reset()保持连接状态；MC_Close()断开网络连接

---

**最佳实践**: 在应用程序启动时调用MC_Reset()，确保控制卡处于干净的初始状态。