# Hardware Limit Invalid - Root Cause Analysis

Created: 2026-01-14
Status: fix_applied
Owner: @claude

## Problem Statement

Hardware limit (hard limit) is not working as expected.

## Root Cause Analysis

### 1. MC_SetHardLimP/N 不适用于4轴控制卡 - 关键发现

**厂商文档证据** (`D:/Projects/Document/BopaiMotionController/full.md`):
```
| MC_SetHardLimP(10轴以上用) | 设置正硬限位映射IO |
| MC_SetHardLimN(10轴以上用) | 设置负硬限位映射IO |
```

**结论:** `MC_SetHardLimP/N` 是给**10轴以上控制卡**用的。对于4轴/8轴控制卡，硬件限位信号是**固定映射**的。

**厂商示例代码** (`DemoVCDlg.cpp`) 只使用 `MC_LmtsOn/Off`，没有调用 `MC_SetHardLimP/N`：
```cpp
void CDemoVCDlg::OnBnClickedCheckPosLimEnable()
{
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 1-based
    if(((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->GetCheck())
    {
        iRes = g_MultiCard.MC_LmtsOn(iAxisNum, MC_LIMIT_POSITIVE);
    }
    else
    {
        iRes = g_MultiCard.MC_LmtsOff(iAxisNum, MC_LIMIT_POSITIVE);
    }
}
```

### 2. SDK Axis Number Convention

SDK uses **1-based axis numbers (1-4)**, NOT 0-based.

### 3. 简化后的调用链

```
ApplicationContainer::ConfigurePorts()
  -> MultiCardMotionAdapter::SetHardLimitPolarity(axis, 1, 1)
     -> sdk_axis = ToSdkAxis(axis)  // 0 -> 1, 1 -> 2, etc.
     -> MC_SetLmtSnsSingle(sdk_axis, 1, 1)  // Low-level trigger
  -> MultiCardMotionAdapter::EnableHardLimits(axis, true, -1)
     -> sdk_axis = ToSdkAxis(axis)
     -> MC_LmtsOn(sdk_axis, -1)  // Enable both directions
  -> MultiCardMotionAdapter::VerifyLimitsEnabled(axis)
     -> sdk_axis = ToSdkAxis(axis)
     -> MC_GetLmtsOnOff(sdk_axis, &pos, &neg)
```

**注意:** 不再调用 `SetHardLimits()` / `MC_SetHardLimP/N`。

## Applied Fixes

### Fix 1: 移除 MC_SetHardLimP/N 调用

**原因:** `MC_SetHardLimP/N` 仅适用于10轴以上控制卡，4轴控制卡的硬限位信号是固定映射的。

**修改位置:** `src/infrastructure/hardware/MultiCardAdapter.cpp:203-223`

**实现:**
```cpp
// 在 MultiCardAdapter::Connect() 中，轴使能完成后配置限位
for (short axis = 1; axis <= 2; ++axis) {  // X轴(1), Y轴(2)
    // 设置极性: 1=低电平触发 (NPN NO型接近开关)
    short polarity_result = multicard_->MC_SetLmtSnsSingle(axis, 1, 1);

    // 启用限位: -1=正负向都启用
    short limit_result = multicard_->MC_LmtsOn(axis, -1);
    if (limit_result == 0) {
        std::cout << "[MultiCardAdapter] Axis " << axis << " hard limits enabled" << std::endl;
    }
}
```

**验证结果 (2026-01-14 15:55:50):**
```
[MultiCardAdapter] Configuring hard limits for axes 1-2...
[MultiCardAdapter] Axis 1 hard limits enabled
[MultiCardAdapter] Axis 2 hard limits enabled
```

### Fix 2: 调用时机修正

**问题:** 在 `ApplicationContainer` 初始化时调用 `MC_LmtsOn` 会失败，因为硬件尚未连接。

**解决方案:** 将限位配置移到 `MultiCardAdapter::Connect()` 中，在 `MC_Open` 和 `MC_AxisOn` 成功后执行。

## Verification Checklist

- [x] 查找厂商示例代码确认API用法
- [x] 确认 MC_SetHardLimP/N 是给10轴以上用的
- [x] 移除 SetHardLimits() 调用
- [x] 编译通过
- [x] 确认限位开关类型 (NO/NC) 与极性设置匹配
- [x] 验证 MC_LmtsOn 返回 0 (成功) - 2026-01-14 15:55:50
- [ ] 验证 MC_GetLmtsOnOff 显示限位已启用
- [ ] 手动触发限位开关测试

## Debug Output to Check

启动应用后，查看以下调试信息：
```
[DEBUG] SetHardLimitPolarity: axis=0 -> sdk_axis=1, pos_polarity=1, neg_polarity=1
[DEBUG] SetHardLimitPolarity: 配置成功 (sdk_axis=1)
[DEBUG] EnableHardLimits: axis=0 -> sdk_axis=1, enable=true, limit_type=-1
[DEBUG] EnableHardLimits: 启用成功 (sdk_axis=1)
[DEBUG] VerifyLimitsEnabled: axis=0 -> sdk_axis=1, pos_enabled=1, neg_enabled=1
```

## 限位开关规格确认

| 属性 | 值 |
|------|-----|
| 型号 | QN-1805-E1 |
| 类型 | NPN NO (常开型) 接近开关 |
| 触发方式 | 低电平触发 (NPN输出) |

**电气接线定义:**
| 线色 | 端子标识 | 功能 |
|------|----------|------|
| BN (棕色) | DC10-30V | 电源正极 (+10~30V) |
| BU (蓝色) | 0V | 电源负极 (GND) |
| BK (黑色) | OUT | 信号输出 (最大 200mA) |

**极性配置匹配确认:**
- 限位开关类型: NPN NO (常开型)
- 正确的极性设置: 1 (低电平触发)
- 当前代码设置: `SetHardLimitPolarity(axis, 1, 1)`
- 状态: 匹配

## 待硬件验证

1. 验证 MC_LmtsOn 返回 0 (成功)
2. 验证 MC_GetLmtsOnOff 显示限位已启用
3. 手动触发限位开关测试
