# MC_AxisOn 函数教程

## 函数原型

```c
int MC_AxisOn(short nAxis)
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

### 示例1：按钮控制轴使能
```cpp
// 通过按钮切换轴的使能状态
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

### 示例2：程序初始化时使能轴
```cpp
// 在程序初始化时使能指定的轴
void InitializeAxis()
{
    int iRes = 0;

    // 使能前3个轴
    for(int i = 1; i <= 3; i++)
    {
        iRes = g_MultiCard.MC_AxisOn(i);
        if(iRes != 0)
        {
            TRACE("轴%d使能失败，错误代码: %d\n", i, iRes);
        }
        else
        {
            TRACE("轴%d使能成功\n", i);
        }
    }
}
```

### 示例3：批量使能多个轴
```cpp
// 批量使能多个轴用于坐标系运动
int EnableMultipleAxes(int* axisList, int count)
{
    int iRes = 0;

    for(int i = 0; i < count; i++)
    {
        iRes = g_MultiCard.MC_AxisOn(axisList[i]);
        if(iRes != 0)
        {
            return iRes;  // 返回第一个失败的错误代码
        }

        // 短暂延时确保使能完成
        Sleep(10);
    }

    return 0;  // 全部成功
}
```

## 参数映射表

| 应用场景 | nAxis | 说明 |
|----------|-------|------|
| 单轴使能 | 1-16 | 指定具体轴号 |
| 程序变量 | m_ComboBoxAxis.GetCurSel()+1 | 从界面选择获取轴号 |
| 批量操作 | 循环变量 | 用于循环使能多个轴 |
| 固定配置 | 常量 | 程序中固定使用的轴 |

## 关键说明

1. **使能条件**：
   - 控制卡必须已连接（MC_Open成功）
   - 轴不能处于报警状态
   - 伺服驱动器必须正常工作
   - 硬限位未触发（如果限位开启）

2. **使能效果**：
   - 轴进入伺服使能状态
   - 电机保持当前位置（力矩保持）
   - 可以接收运动命令
   - 编码器反馈生效

3. **使用时机**：
   - **程序初始化**：在配置完参数后使能
   - **手动控制**：用户点击使能按钮时
   - **错误恢复**：清除报警后重新使能
   - **坐标系准备**：多轴插补前使能相关轴

4. **注意事项**：
   - 使能前确保轴在安全位置
   - 监控使能后的轴状态
   - 处理使能失败的情况
   - 考虑使能延时和稳定性

## 函数区别

- **MC_AxisOn() vs MC_AxisOff()**: MC_AxisOn()使能轴，MC_AxisOff()断开使能
- **MC_AxisOn() vs MC_PrfTrap()**: MC_AxisOn()是硬件使能，MC_PrfTrap()是运动模式设置
- **MC_AxisOn() vs MC_ClrSts()**: MC_AxisOn()激活轴，MC_ClrSts()清除轴状态

---

**安全建议**: 在使能轴之前，确保轴在安全位置且不会造成危险。建议添加限位检查和状态监控。