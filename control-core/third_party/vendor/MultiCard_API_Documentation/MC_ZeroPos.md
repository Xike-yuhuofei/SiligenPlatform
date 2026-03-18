# MC_ZeroPos 函数教程

## 函数原型

```c
int MC_ZeroPos(short nAxis)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nAxis | short | 轴号，取值范围：1-16 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：单轴位置清零
```cpp
// 通过按钮将当前轴的位置清零
void CDemoVCDlg::OnBnClickedButton4()
{
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号
    g_MultiCard.MC_ZeroPos(iAxisNum);  // 将当前位置设为零点
}
```

### 示例2：批量清零多个轴
```cpp
// 将多个轴的位置同时清零（通常在建立坐标系时使用）
void ZeroMultipleAxes()
{
    int axes[] = {1, 2, 3};  // 要清零的轴号
    int count = sizeof(axes) / sizeof(axes[0]);

    for(int i = 0; i < count; i++)
    {
        int iRes = g_MultiCard.MC_ZeroPos(axes[i]);
        if(iRes == 0)
        {
            TRACE("轴%d位置清零成功\n", axes[i]);
        }
        else
        {
            TRACE("轴%d位置清零失败，错误代码: %d\n", axes[i], iRes);
        }
    }
}
```

### 示例3：回零后设置原点
```cpp
// 在回零操作完成后设置当前位置为零点
void AfterHomingOperation(int axis)
{
    // 等待回零完成
    short nCrdStatus;
    do {
        g_MultiCard.MC_CrdStatus(1, &nCrdStatus, NULL);
        Sleep(100);
    } while(nCrdStatus & CRDSYS_STATUS_PROG_RUN);

    // 回零完成后将当前位置设为零点
    int iRes = g_MultiCard.MC_ZeroPos(axis);
    if(iRes == 0)
    {
        TRACE("轴%d回零完成，原点设置成功\n", axis);
    }
}
```

## 参数映射表

| 应用场景 | nAxis | 说明 |
|----------|-------|------|
| 单轴清零 | 1-16 | 指定具体轴号 |
| 界面选择 | m_ComboBoxAxis.GetCurSel()+1 | 从用户界面选择轴 |
| 批量操作 | 数组元素 | 批量处理多个轴 |
| 回零后处理 | 回零轴号 | 在回零完成后设置原点 |

## 关键说明

1. **清零效果**：
   - 将当前位置寄存器设置为0
   - 不影响轴的实际物理位置
   - 后续运动指令以新零点为基准
   - 编码器计数被重新校准

2. **使用时机**：
   - **回零完成后**：机械回零后设置电子零点
   - **手动定位后**：将轴移动到期望原点后清零
   - **坐标系建立前**：多轴协调运动前统一清零
   - **位置校准时**：定期校准设备原点

3. **注意事项**：
   - 清零前确保轴在稳定位置
   - 避免在运动过程中执行清零
   - 清零后需要更新界面显示
   - 考虑清零对已有坐标系的影响

4. **与回零的区别**：
   - MC_ZeroPos()是软件设置零点
   - MC_HomeStart()是机械回零操作
   - 通常先机械回零，再软件清零
   - 软件零点可以随时重新设置

## 函数区别

- **MC_ZeroPos() vs MC_HomeStart()**: MC_ZeroPos()是软件清零，MC_HomeStart()是机械回零
- **MC_ZeroPos() vs MC_SetPos()**: MC_ZeroPos()设置当前位置为零，MC_SetPos()设置目标位置
- **MC_ZeroPos() vs MC_ClrSts()**: MC_ZeroPos()清零位置，MC_ClrSts()清除状态标志

---

**最佳实践**: 在执行重要位置操作前，先确认轴的位置和状态，避免在错误的位置执行清零操作。