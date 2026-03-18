# 硬件限位诊断测试程序使用指南

> 注意：`limit_switch_test` 示例已移除，不再构建。本文档仅供历史参考。

## 概述

`limit_switch_test` 是一个独立的硬件限位功能测试程序,用于验证 HOME 输入上的限位开关是否正常工作。

## 功能特性

- 连接到 MultiCard 控制卡
- 直接读取 HOME/LIM/GPI 原始IO
- 实时监控限位触发状态(按位变化输出)
- 清晰的诊断输出
- 支持自定义 IP/端口与采样周期

## 编译方法

已移除，不再提供编译目标。

## 使用方法（历史参考）

### 基本用法

```bash
limit_switch_test.exe [options]
```

### 参数说明

- `--pc-ip`: 本机IP
- `--card-ip`: 控制卡IP
- `--pc-port`: 本机端口
- `--card-port`: 控制卡端口
- `--interval-ms`: 采样周期(默认5ms)
- `--print-ms`: 定期输出间隔(默认1000ms)

### 使用示例

```bash
# 使用默认配置
limit_switch_test.exe

# 自定义IP与采样周期
limit_switch_test.exe --pc-ip 192.168.0.200 --card-ip 192.168.0.1 --interval-ms 5
```

## 测试流程

1. **连接控制卡**
   - 程序会自动连接到配置的控制卡
   - 默认地址: 192.168.0.200:0 / 192.168.0.1:0

2. **监控限位状态**
   - 实时输出 HOME/LIM/GPI 原始值
   - 发现位变化时立即输出

3. **断开连接**
   - 退出程序自动断开控制卡连接

## 输出示例

```
Connecting: card=1 pc=192.168.0.200:0 card=192.168.0.1:0
MC_Reset ok
Reading DI (HOME/LIM+/LIM-/GPI). Axis1=X bit0, Axis2=Y bit1. Press 'q' to quit.
[    1017 ms] HOME ret=0 raw=0xFFFFFFF1 X(bit=1 hi=1 lo=0) Y(bit=0 hi=0 lo=1)
             LIM+ ret=0 raw=0x0000000F X=1 Y=1 | LIM- ret=0 raw=0x0000000F X=1 Y=1 | GPI ret=0 raw=0x00000000
```

## 测试步骤

1. **准备工作**
   - 确保控制卡已上电
   - 确保网络连接正常
   - 确保限位开关已正确接线(HOME1/2)

2. **运行程序**
   ```bash
   limit_switch_test.exe
   ```

3. **手动触发限位**
   - 程序开始监控后,手动触发限位开关
   - 观察 HOME 位变化:
     - X 轴: bit0
     - Y 轴: bit1

4. **验证结果**
   - 如果 HOME 位变化正确,说明限位接线与极性正常
   - 若只有 LIM+/LIM- 变化,说明接线不在 HOME 端子

## 故障排查

### 连接失败

**现象**: `MC_Open failed`

**可能原因**:
- 控制卡未上电
- 网络连接问题
- IP地址配置错误

**解决方法**:
1. 检查控制卡电源
2. 使用 `ping 192.168.0.1` 测试网络连通性
3. 确认 IP 地址配置正确

### 限位无响应

**现象**: 触发限位开关时,状态不变化

**可能原因**:
- 限位开关接线错误
- 限位开关类型不匹配
- 读取的DI类型不正确

**解决方法**:
1. 检查限位开关接线 (信号线、电源线、地线)
2. 确认限位开关类型 (NO 常开)
3. 使用 `MC_GetDiRaw(MC_HOME)` 验证 HOME 位是否变化

## 代码复用说明

1. **MultiCard SDK API**
   - `MC_Open` - 连接控制卡
   - `MC_Reset` - 复位控制卡
   - `MC_GetDiRaw` - 读取 HOME/LIM/GPI 原始值
   - `MC_Close` - 断开连接

2. **状态位定义**
- 来自 `modules/device-hal/src/drivers/multicard/MultiCardCPP.h`
   - `MC_HOME` / `MC_LIMIT_POSITIVE` / `MC_LIMIT_NEGATIVE` / `MC_GPI`

3. **连接配置**
   - 参考 `docs/library/06_reference.md`
   - IP/端口与现场配置保持一致
