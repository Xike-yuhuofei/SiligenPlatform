# MC_Stop 函数教程

## 函数原型

```c
int MC_Stop(long lAxisMask, long lCrdMask)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| lAxisMask | long | 轴掩码，指定要停止的轴。bit0对应轴1，bit1对应轴2，以此类推 |
| lCrdMask | long | 坐标系掩码，指定要停止的坐标系。bit0对应坐标系1，bit1对应坐标系2，以此类推 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：停止所有轴和坐标系
```cpp
// 停止所有轴（0xFFFFFFFF）和所有坐标系（0xFFFFFFFF）
g_MultiCard.MC_Stop(0XFFFFFFFF,0XFFFFFFFF);
```

### 示例2：JOG模式停止（单轴）
```cpp
// 在JOG按钮松开时停止运动（只停止轴，坐标系掩码为0xFFFF）
if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_NEG)->m_hWnd)
{
    g_MultiCard.MC_Stop(0XFFFF,0XFFFF);  // 停止轴运动
}
if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_POS)->m_hWnd)
{
    g_MultiCard.MC_Stop(0XFFFF,0XFFFF);  // 停止轴运动
}
```

### 示例3：停止坐标系运动
```cpp
// 停止所有坐标系运动（坐标系掩码0xFFFFFF00，轴掩码为0）
void CDemoVCDlg::OnBnClickedButton12()
{
    g_MultiCard.MC_Stop(0XFFFFFF00,0XFFFFFF00);
}
```

## 参数映射表

| 停止对象 | lAxisMask | lCrdMask | 说明 |
|----------|-----------|----------|------|
| 所有轴和坐标系 | 0xFFFFFFFF | 0xFFFFFFFF | 完全停止 |
| 仅轴运动 | 0xFFFF | 0xFFFF | 停止单轴运动，不影响坐标系 |
| 仅坐标系运动 | 0 | 0xFFFFFF00 | 停止插补运动，不影响单轴 |
| 指定轴 | 0x0001 | 0 | 只停止轴1（bit0） |
| 指定坐标系 | 0 | 0x0001 | 只停止坐标系1（bit0） |

## 关键说明

1. **掩码计算**：
   - **轴掩码**：0xFFFFFFFF表示所有16个轴，0x0001表示轴1，0x0002表示轴2
   - **坐标系掩码**：0xFFFFFFFF表示所有坐标系，0x0001表示坐标系1
   - 掩码中对应的bit位为1表示停止该对象

2. **停止行为**：
   - **立即停止**：按照设定的减速度停止，不是急停
   - **保留配置**：停止运动但保留运动参数和配置
   - **状态更新**：轴状态会更新为停止状态

3. **使用场景**：
   - **紧急停止**：用户按下停止按钮
   - **JOG控制**：按钮松开时停止连续运动
   - **程序控制**：条件满足时停止运动
   - **错误处理**：检测到异常时停止

4. **注意事项**：
   - 停止后轴仍保持使能状态
   - 位置信息不会丢失
   - 可以直接重新启动运动
   - 与MC_Reset()不同，配置参数不会被清除

## 函数区别

- **MC_Stop() vs MC_Reset()**: MC_Stop()停止运动但保留配置；MC_Reset()完全重置控制卡
- **MC_Stop() vs MC_Close()**: MC_Stop()保持连接状态；MC_Close()断开连接
- **MC_Stop(轴) vs MC_Stop(坐标系)**: 分别控制单轴运动和多轴插补运动

---

**编程技巧**: 在JOG模式中，建议在按钮按下时启动运动，按钮松开时调用MC_Stop()停止运动。