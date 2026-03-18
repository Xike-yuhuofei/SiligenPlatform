# MC_SetOverride 函数教程

## 函数原型

```c
int MC_SetOverride(short nCrdNum, double synVelRatio)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| synVelRatio | double | 速度倍率，1.0表示100%原始速度 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本速度覆盖控制
```cpp
// 通过滑块控制坐标系速度覆盖
void CDemoVCDlg::OnNMReleasedcaptureSliderOverRide(NMHDR *pNMHDR, LRESULT *pResult)
{
    double dOverRide = 0;

    // 从滑块获取速度倍率（0-200%）
    dOverRide = m_SliderCrdVel.GetPos() / 100.00;

    // 设置坐标系速度覆盖
    int iRes = g_MultiCard.MC_SetOverride(1, dOverRide);

    if(iRes == 0)
    {
        CString strText;
        strText.Format("速度覆盖设置为: %.1f%%", dOverRide * 100);
        GetDlgItem(IDC_STATIC_CRD_VEL_OVER_RIDE)->SetWindowText(strText);
        TRACE("坐标系1速度覆盖设置成功: %.2f\n", dOverRide);
    }
    else
    {
        TRACE("坐标系1速度覆盖设置失败，错误代码: %d\n", iRes);
        AfxMessageBox("速度覆盖设置失败！");
    }

    *pResult = 0;
}
```

### 示例2：智能速度调整
```cpp
// 根据系统状态智能调整速度覆盖
void IntelligentSpeedAdjustment(int crdNum)
{
    double currentVel = 0;
    long crdSpace = 0;

    int iRes1 = g_MultiCard.MC_GetCrdVel(crdNum, &currentVel);
    int iRes2 = g_MultiCard.MC_CrdSpace(crdNum, &crdSpace, 0);

    if(iRes1 != 0 || iRes2 != 0) return;

    // 智能速度调整策略
    double newOverride = 1.0;  // 默认100%速度

    if(crdSpace < 50)
    {
        // 缓冲区空间不足，降低速度
        newOverride = 0.5;  // 降低到50%
        TRACE("缓冲区空间不足，降低速度到50%%\n");
    }
    else if(crdSpace > 300)
    {
        // 缓冲区空间充足，可以提高速度
        newOverride = 1.2;  // 提高到120%
        TRACE("缓冲区空间充足，提高速度到120%%\n");
    }

    // 根据当前速度微调
    if(currentVel > 1000)
    {
        newOverride *= 0.8;  // 进一步降低20%
        TRACE("当前速度过高，进一步降低速度\n");
    }
    else if(currentVel < 100)
    {
        newOverride *= 1.1;  // 略微提高10%
        TRACE("当前速度过低，略微提高速度\n");
    }

    // 限制速度范围在20%-200%之间
    if(newOverride < 0.2) newOverride = 0.2;
    if(newOverride > 2.0) newOverride = 2.0;

    // 应用新的速度覆盖
    int iRes = g_MultiCard.MC_SetOverride(crdNum, newOverride);
    if(iRes == 0)
    {
        TRACE("智能速度调整完成，新的速度覆盖: %.1f%%\n", newOverride * 100);
        UpdateSpeedDisplay(crdNum, newOverride);
    }
}
```

### 示例3：渐进式速度变化
```cpp
// 实现平滑的速度过渡，避免突变
void SmoothSpeedTransition(int crdNum, double targetOverride, int transitionTimeMs)
{
    static double currentOverride = 1.0;
    static CTimer* transitionTimer = NULL;

    if(transitionTimer == NULL)
    {
        transitionTimer = new CTimer();
    }

    // 计算速度变化步长
    double totalChange = targetOverride - currentOverride;
    int steps = 10;  // 分10步完成过渡
    double stepChange = totalChange / steps;
    int stepInterval = transitionTimeMs / steps;

    // 如果变化量很小，直接设置
    if(fabs(totalChange) < 0.01)
    {
        g_MultiCard.MC_SetOverride(crdNum, targetOverride);
        currentOverride = targetOverride;
        TRACE("速度覆盖直接设置为: %.1f%%\n", targetOverride * 100);
        return;
    }

    // 开始渐进式速度调整
    TRACE("开始渐进式速度调整: %.1f%% -> %.1f%%\n",
          currentOverride * 100, targetOverride * 100);

    for(int i = 1; i <= steps; i++)
    {
        double stepOverride = currentOverride + stepChange * i;

        // 使用定时器延迟设置每步速度
        SetTimer(TIMER_SPEED_TRANSITION + i, stepInterval * i, NULL);

        // 存储每步的参数（实际应用中需要更好的数据结构）
        m_speedTransitionSteps[i] = stepOverride;
    }

    currentOverride = targetOverride;
}

// 定时器回调中执行速度调整
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    if(nIDEvent >= TIMER_SPEED_TRANSITION && nIDEvent <= TIMER_SPEED_TRANSITION + 10)
    {
        int stepIndex = nIDEvent - TIMER_SPEED_TRANSITION;
        if(m_speedTransitionSteps.find(stepIndex) != m_speedTransitionSteps.end())
        {
            double stepOverride = m_speedTransitionSteps[stepIndex];
            g_MultiCard.MC_SetOverride(1, stepOverride);
            TRACE("速度调整步骤%d: %.1f%%\n", stepIndex, stepOverride * 100);
        }
    }
}
```

### 示例4：多坐标系同步速度控制
```cpp
// 同步控制多个坐标系的速度覆盖
void SynchronizedSpeedControl(int crdCount, double masterOverride)
{
    double individualOverrides[16] = {0};

    // 为每个坐标系计算独立的速度覆盖
    for(int i = 0; i < crdCount; i++)
    {
        int crdNum = i + 1;

        // 根据各坐标系的状态调整速度
        double currentVel = 0;
        long bufferSpace = 0;

        g_MultiCard.MC_GetCrdVel(crdNum, &currentVel);
        g_MultiCard.MC_CrdSpace(crdNum, &bufferSpace, 0);

        // 基础速度覆盖
        individualOverrides[i] = masterOverride;

        // 根据缓冲区状态微调
        if(bufferSpace < 100)
            individualOverrides[i] *= 0.7;  // 缓冲区紧张，降低30%
        else if(bufferSpace > 400)
            individualOverrides[i] *= 1.1;  // 缓冲区充足，提高10%

        // 根据当前速度微调
        if(currentVel > 800)
            individualOverrides[i] *= 0.9;  // 速度过高，降低10%
        else if(currentVel < 50)
            individualOverrides[i] *= 1.2;  // 速度过低，提高20%

        // 限制范围
        if(individualOverrides[i] < 0.1) individualOverrides[i] = 0.1;
        if(individualOverrides[i] > 2.0) individualOverrides[i] = 2.0;
    }

    // 同步设置所有坐标系的速度覆盖
    for(int i = 0; i < crdCount; i++)
    {
        int crdNum = i + 1;
        int iRes = g_MultiCard.MC_SetOverride(crdNum, individualOverrides[i]);

        if(iRes == 0)
        {
            TRACE("坐标系%d速度覆盖设置: %.1f%%\n",
                  crdNum, individualOverrides[i] * 100);
        }
        else
        {
            TRACE("坐标系%d速度覆盖设置失败\n", crdNum);
        }
    }
}
```

### 示例5：速度覆盖安全控制
```cpp
// 带安全检查的速度覆盖设置
bool SafeSpeedOverride(int crdNum, double requestedOverride)
{
    // 安全检查参数
    const double MAX_OVERRIDE = 3.0;      // 最大300%速度
    const double MIN_OVERRIDE = 0.1;      // 最小10%速度
    const double WARNING_OVERRIDE = 2.0;  // 警告阈值200%

    // 参数范围检查
    if(requestedOverride < MIN_OVERRIDE || requestedOverride > MAX_OVERRIDE)
    {
        TRACE("错误：速度覆盖参数超出安全范围 %.1f-%.1f\n",
              MIN_OVERRIDE * 100, MAX_OVERRIDE * 100);
        return false;
    }

    // 获取当前系统状态
    short crdStatus = 0;
    double currentVel = 0;
    long crdSpace = 0;

    g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);
    g_MultiCard.MC_GetCrdVel(crdNum, &currentVel);
    g_MultiCard.MC_CrdSpace(crdNum, &crdSpace, 0);

    // 安全条件检查
    if(crdStatus & CRDSYS_STATUS_ALARM)
    {
        TRACE("警告：坐标系处于报警状态，不允许调整速度覆盖\n");
        return false;
    }

    if(!(crdStatus & CRDSYS_STATUS_PROG_RUN))
    {
        TRACE("信息：坐标系未运行，速度覆盖设置可能无效\n");
    }

    // 高速安全检查
    if(requestedOverride > WARNING_OVERRIDE)
    {
        TRACE("警告：设置高速覆盖 %.1f%%，请确认系统安全\n",
              requestedOverride * 100);

        // 检查机械和电气条件
        if(!IsSystemSafeForHighSpeed())
        {
            TRACE("安全检查失败：系统不适合高速运行\n");
            return false;
        }
    }

    // 应用速度覆盖
    int iRes = g_MultiCard.MC_SetOverride(crdNum, requestedOverride);
    if(iRes == 0)
    {
        TRACE("安全速度覆盖设置成功：坐标系%d -> %.1f%%\n",
              crdNum, requestedOverride * 100);

        // 记录设置日志
        LogSpeedOverrideChange(crdNum, requestedOverride, currentVel, crdSpace);
        return true;
    }
    else
    {
        TRACE("错误：速度覆盖设置失败，错误代码: %d\n", iRes);
        return false;
    }
}
```

### 示例6：速度覆盖预设管理
```cpp
// 管理常用的速度覆盖预设
class SpeedOverrideManager {
private:
    struct Preset {
        CString name;
        double ratio;
        CString description;
    };

    std::vector<Preset> m_presets;
    double m_currentOverride;

public:
    SpeedOverrideManager() {
        // 初始化预设配置
        m_presets.push_back({"停止", 0.0, "完全停止运动"});
        m_presets.push_back({"爬行", 0.1, "极低速爬行"});
        m_presets.push_back({"低速", 0.3, "低速精密运行"});
        m_presets.push_back({"中速", 0.6, "中速常规运行"});
        m_presets.push_back({"标准", 1.0, "标准速度运行"});
        m_presets.push_back({"快速", 1.5, "高速运行"});
        m_presets.push_back({"极速", 2.0, "最高速度运行"});
    }

    bool ApplyPreset(int crdNum, const CString& presetName) {
        for(const auto& preset : m_presets) {
            if(preset.name == presetName) {
                return ApplyOverride(crdNum, preset.ratio, preset.name);
            }
        }
        TRACE("未找到预设: %s\n", presetName);
        return false;
    }

    bool ApplyOverride(int crdNum, double ratio, const CString& reason = "") {
        if(SafeSpeedOverride(crdNum, ratio)) {
            m_currentOverride = ratio;
            if(!reason.IsEmpty()) {
                TRACE("应用速度覆盖: %s (%.1f%%) - %s\n",
                      reason, ratio * 100, GetOverrideDescription(ratio));
            }
            return true;
        }
        return false;
    }

    CString GetOverrideDescription(double ratio) {
        if(ratio == 0.0) return "停止";
        if(ratio < 0.3) return "低速";
        if(ratio < 0.7) return "中速";
        if(ratio < 1.2) return "标准";
        if(ratio < 1.8) return "快速";
        return "极速";
    }

    double GetCurrentOverride() const { return m_currentOverride; }
    std::vector<CString> GetPresetNames() const {
        std::vector<CString> names;
        for(const auto& preset : m_presets) {
            names.push_back(preset.name);
        }
        return names;
    }
};

// 使用示例
void UseSpeedPresets(int crdNum) {
    static SpeedOverrideManager speedManager;

    // 应用低速预设
    speedManager.ApplyPreset(crdNum, "低速");

    // 或者直接设置特定速度
    speedManager.ApplyOverride(crdNum, 0.8, "临时调整");
}
```

## 参数映射表

| 应用场景 | nCrdNum | synVelRatio | 说明 |
|----------|---------|-------------|------|
| 手动控制 | 1-16 | 0.1-2.0 | 用户界面滑块控制 |
| 智能调整 | 坐标系号 | 计算值 | 根据系统状态自动调整 |
| 安全控制 | 指定坐标系 | 安全范围值 | 带安全检查的设置 |
| 预设管理 | 固定坐标系 | 预设值 | 常用速度预设管理 |

## 关键说明

1. **速度倍率**：
   - 1.0表示100%原始速度
   - 小于1.0表示减速（如0.5=50%速度）
   - 大于1.0表示加速（如1.5=150%速度）

2. **实时性**：
   - 可以在运动过程中实时调整
   - 立即生效，无需重启运动
   - 影响后续所有运动段

3. **应用范围**：
   - 只影响坐标系的合成速度
   - 不改变单轴的最大速度限制
   - 保持各轴的速度比例关系

4. **安全考虑**：
   - 设置合理的速度范围限制
   - 高速运行前进行安全检查
   - 考虑机械和电气限制

5. **使用场景**：
   - **手动调速**：操作员实时调整速度
   - **自动优化**：系统根据状态自动调速
   - **精密控制**：低速精密作业
   - **高速运行**：提高生产效率

6. **注意事项**：
   - 过高的速度可能影响精度
   - 频繁的速度调整可能影响平滑性
   - 需要考虑加减速性能限制

## 函数区别

- **MC_SetOverride() vs MC_SetVel()**: 速度倍率 vs 绝对速度
- **MC_SetOverride() vs MC_G00SetOverride()**: 坐标系速度 vs G00速度
- **速度覆盖 vs 参数设置**: 实时调整 vs 配置参数

---

**使用建议**: 在需要实时速度控制的应用中，建议建立速度覆盖管理系统，包括预设配置、安全检查和智能调整功能。对于高速应用，务必进行充分的安全评估。定期监控速度覆盖的效果，确保系统稳定性和运动质量。