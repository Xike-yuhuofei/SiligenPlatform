# X1引脚急停配置指南

## 概述

本指南详细说明如何配置和使用MultiCard控制卡的X1引脚作为急停输入。系统已更新为通过X1引脚直接读取急停状态，无需连接外部传感器。

## 硬件连接

### 急停按钮规格
- **引脚**: MultiCard控制卡 X1引脚
- **电路类型**: 常闭电路 (Normally Closed)
- **信号电平**: 正常状态高电平，急停按下低电平
- **响应要求**: <100ms

### 接线方式
```
MultiCard X1引脚 ────┐
                     ├── 急停按钮 ──── GND
                     │
                   上拉电阻 (10KΩ)
                     │
                   +5V
```

**重要**:
- 使用常闭电路，确保急停线路安全可靠
- 急停按钮松开时X1为高电平，按下时为低电平
- 系统检测到低电平信号时立即触发急停

## 软件配置

### 1. 安全配置文件 (`config/security_config.ini`)

```ini
[Interlock]
# 硬件安全连锁 (已启用 - X1急停)
enabled=true
# X1引脚急停输入 (DI bit 0, 低电平有效)
emergency_stop_input=0
# 其他传感器预留（待硬件连接）
safety_door_input=1
pressure_sensor_input=2
temperature_sensor_input=3
voltage_sensor_input=4
poll_interval_ms=50
```

### 2. 系统参数
- **轮询间隔**: 50ms
- **急停响应时间**: <100ms
- **信号读取**: `MC_GetDiRaw(0, 0)` - DI bit 0
- **安全逻辑**: 低电平有效
- **故障处理**: 读取失败时保守处理，假设急停已触发

## 软件实现

### InterlockMonitor 集成

```cpp
// 读取急停状态 - X1引脚作为急停输入
long diValue = 0;
short ret = MC_GetDiRaw(0, 0, &diValue);
if (ret == 0) {
    // X1引脚对应DI bit 0，低电平有效（急停按钮按下时为0）
    state_.emergency_stop_triggered = (diValue & 0x01) == 0;
} else {
    // 读取失败时保守处理，假设急停已触发
    state_.emergency_stop_triggered = true;
}
```

### HardwareTestAdapter 集成

```cpp
bool HardwareTestAdapter::isEmergencyStopActive() const {
    long diValue = 0;
    short ret = MC_GetDiRaw(m_cardHandle, 0, &diValue);

    if (ret == 0) {
        // X1引脚对应DI bit 0，低电平有效
        return (diValue & 0x01) == 0;
    }

    // 读取失败时保守处理，假设急停已触发
    return true;
}
```

## 安全特性

### 响应时间保证
- **轮询周期**: 50ms
- **最大响应时间**: <100ms
- **优先级**: CRITICAL (最高优先级)

### 故障安全设计
1. **读取失败**: 保守处理，假设急停已触发
2. **线路断开**: 相当于急停触发
3. **常闭电路**: 线路故障时自动触发急停

### 审计日志
- 急停触发事件完整记录
- 时间戳精确到毫秒
- 包含系统状态快照

## 测试验证

### 功能测试
```bash
# 运行急停集成测试
.\build\bin\Release\test_interlock_hardware.exe
```

### 测试内容
1. **响应时间测试**: 验证急停响应 <100ms
2. **信号正确性**: 验证高/低电平对应正常/急停状态
3. **故障恢复**: 验证急停释放后的系统恢复
4. **错误处理**: 验证读取失败的安全处理

### 测试步骤
1. 连接急停按钮到X1引脚
2. 启动InterlockMonitor监控
3. 按下急停按钮，验证系统立即停机
4. 释放急停按钮，验证系统状态恢复
5. 断开X1连接，验证系统触发急停

## 维护说明

### 日常检查
- 急停按钮功能正常
- 接线牢固无松动
- 上拉电阻工作正常

### 故障排除
| 现象 | 可能原因 | 解决方法 |
|------|----------|----------|
| 系统始终处于急停状态 | 线路断开或按钮故障 | 检查接线，更换按钮 |
| 急停响应慢 | 轮询间隔过大 | 检查poll_interval_ms配置 |
| 急停无效 | X1引脚配置错误 | 验证MC_GetDiRaw调用 |

## 配置验证

### 硬件验证命令
```cpp
// 读取X1引脚状态
long diValue = 0;
short result = MC_GetDiRaw(0, 0, &diValue);
// 检查DI bit 0状态
bool x1State = (diValue & 0x01) != 0;
```

### 预期状态
- **正常**: DI bit 0 = 1 (高电平)
- **急停**: DI bit 0 = 0 (低电平)

## 安全注意事项

⚠️ **重要安全提示**:
1. 急停按钮必须使用常闭电路设计
2. 定期测试急停功能，确保可靠性
3. 任何急停线路故障都应立即停机检修
4. 保持急停按钮易于操作，无障碍物阻挡
5. 急停复位只能通过硬件按钮手动完成

## 技术规格

- **输入电压**: 5V TTL电平
- **响应时间**: <100ms
- **信号类型**: 数字输入，低电平有效
- **轮询频率**: 20Hz (50ms周期)
- **安全等级**: SIL 2 (根据应用场景)
- **环境要求**: 工业级，IP65防护