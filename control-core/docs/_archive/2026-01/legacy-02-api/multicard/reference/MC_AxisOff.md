# MC_AxisOff 函数教程

## 函数原型

```c
int MC_AxisOff(short nAxis)
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

### 示例1：按钮控制轴断开使能
```cpp
// 通过按钮切换轴的使能/断开状态
int iRes = 0;
CString strText;
int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号

GetDlgItemText(IDC_BUTTON_AXIS_ON,strText);
if (strText == _T("伺服使能"))
{
    iRes = g_MultiCard.MC_AxisOn(iAxisNum);  // 使能轴
    GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText(_T("断开使能"));
}
else
{
    iRes = g_MultiCard.MC_AxisOff(iAxisNum);  // 断开使能
    GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText(_T("伺服使能"));
}
```

### 示例2：程序退出时断开所有轴
```cpp
// 在程序退出前断开所有轴的使能
void DisableAllAxes()
{
    int iRes = 0;

    for(int i = 1; i <= 16; i++)
    {
        iRes = g_MultiCard.MC_AxisOff(i);
        if(iRes != 0)
        {
            TRACE("轴%d断开使能失败，错误代码: %d\n", i, iRes);
        }
        else
        {
            TRACE("轴%d断开使能成功\n", i);
        }
    }
}
```

### 示例3：紧急停止处理
```cpp
// 在紧急停止情况下断开所有轴
void EmergencyStop()
{
    // 首先停止所有运动
    g_MultiCard.MC_Stop(0xFFFFFFFF, 0xFFFFFFFF);

    // 然后断开所有轴的使能
    for(int i = 1; i <= 16; i++)
    {
        g_MultiCard.MC_AxisOff(i);
    }

    MessageBox("紧急停止已执行，所有轴已断开使能！");
}
```

## 参数映射表

| 应用场景 | nAxis | 说明 |
|----------|-------|------|
| 单轴控制 | 1-16 | 指定具体轴号 |
| 界面选择 | m_ComboBoxAxis.GetCurSel()+1 | 从用户界面获取轴号 |
| 批量操作 | 循环变量 | 用于循环断开多个轴 |
| 安全停机 | 全部轴 | 程序退出或紧急情况时使用 |

## 关键说明

1. **断开效果**：
   - 轴进入伺服断开状态
   - 电机失去力矩保持（可能自由移动）
   - 不再响应运动命令
   - 编码器反馈可能仍然有效

2. **使用时机**：
   - **程序退出**：确保设备安全停机
   - **紧急停止**：危险情况下立即停止
   - **维护模式**：设备维护和调试时
   - **错误处理**：严重错误时断开使能

3. **安全考虑**：
   - 断开使能后轴可能因重力或外力移动
   - 垂直轴需要机械制动或其他安全措施
   - 确保断开使能不会造成设备损坏或人员危险
   - 在断开前先停止所有运动

4. **注意事项**：
   - 断开使能不会清除运动参数
   - 重新使能时轴可能不在原位置
   - 建议在断开前记录当前位置（如需要）
   - 断开使能后状态标志会相应更新

## 函数区别

- **MC_AxisOff() vs MC_AxisOn()**: MC_AxisOff()断开使能，MC_AxisOn()使能轴
- **MC_AxisOff() vs MC_Stop()**: MC_AxisOff()断开伺服，MC_Stop()停止运动但保持使能
- **MC_AxisOff() vs MC_Reset()**: MC_AxisOff()只影响单个轴，MC_Reset()影响整个控制卡

---

**安全提示**: 对于垂直轴或有重力影响的轴，断开使能前确保有适当的机械制动或支撑措施。