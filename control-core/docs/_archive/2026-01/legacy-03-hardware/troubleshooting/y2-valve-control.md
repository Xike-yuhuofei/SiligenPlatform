# Y2引脚控制胶筒气压电磁阀 - 端到端测试方案

## 概述

本文档提供Y2输出引脚控制胶筒气压电磁阀的完整端到端测试方案，参考`test\test_CmpPluse.cpp`实现风格，基于真实硬件测试，禁止仿真。

**硬件配置**:
- 控制卡IP: 192.168.0.1
- Y2输出引脚 → 电磁阀继电器
- 气源压力: 50 kPa (配置于`config/machine_config.ini`)
- 电源: 24V DC

---

## 1. 硬件连接验证

**测试目标**: 验证Y2引脚与电磁阀物理连接正常

**前置条件**:
- 24V电源稳定供电
- 控制卡IP: 192.168.0.1 可ping通
- Y2输出引脚已接电磁阀继电器
- 电磁阀气源压力设定在配置范围内(50kPa)

**测试步骤**:
```
1. 检查硬件接线
   - Y2输出 → 继电器线圈+
   - 继电器线圈- → GND
   - 继电器常开触点 → 电磁阀控制端
   - 电磁阀电源 → 24V DC

2. 万用表测试
   - 测量Y2输出电压(断开状态应为0V)
   - 测量电磁阀端电压(无信号时0V)
   - 测量气源压力(50±5 kPa)
```

**预期结果**: 所有电压/压力值在正常范围内

---

## 2. 单次脉冲输出测试

**测试目标**: 验证Y2引脚可发送单个脉冲

**测试程序**: `test\test_y2_single_pulse.cpp`

**测试参数**:
```cpp
// Y2对应通道2 (通道1=Y1, 通道2=Y2)
short nChannelMask = 2;        // 通道2 (二进制: 0b10)
short nPluseType   = 2;        // 脉冲模式
short nTime1       = 2000;     // 脉冲宽度: 2ms (2000μs)
short nStartLevel  = 0;        // 起始电平: 低
```

**测试代码框架**:
```cpp
#include "utils/TestHelper.h"

int main() {
    TestHelper::InitConsole();
    TestHelper::PrintHeader("Y2单次脉冲测试");

    MultiCard card;
    if (TestHelper::ConnectCard(card) != 0) return 1;

    // 测试1: 发送2ms脉冲
    std::cout << "[TEST1] 发送2ms脉冲到Y2..." << std::endl;
    int ret = card.MC_CmpPluse(2, 2, 0, 2000, 0, 0, 0);

    if (ret == 0) {
        std::cout << "[SUCCESS] 脉冲发送成功" << std::endl;
        std::cout << "[观察] 电磁阀应开启2ms后关闭" << std::endl;
    } else {
        std::cout << "[ERROR] 脉冲发送失败: " << ret << std::endl;
    }

    // 等待用户确认
    std::cout << "\n是否观察到电磁阀动作? (y/n): ";
    char confirm;
    std::cin >> confirm;

    card.MC_Close();
    TestHelper::WaitExit();
    return (ret == 0 && confirm == 'y') ? 0 : 1;
}
```

**预期结果**:
- 函数返回0
- 电磁阀开启2ms后自动关闭
- 可听到"嘭"的气流声

**失败诊断**:
- 返回-6: 检查控制卡连接
- 返回1: 检查通道参数是否正确
- 无动作: 检查Y2接线/继电器/电磁阀

---

## 3. 多次脉冲输出测试

**测试目标**: 验证Y2可连续发送多个脉冲

**测试程序**: `test\test_y2_multiple_pulses.cpp`

**测试参数**:
```cpp
struct TestCase {
    const char* name;
    short pulse_width_us;    // 脉冲宽度(微秒)
    short interval_ms;       // 间隔时间(毫秒)
    int repeat_count;        // 重复次数
};

TestCase cases[] = {
    {"短脉冲快速重复", 1000, 100, 10},   // 1ms脉冲,100ms间隔,10次
    {"标准脉冲",      2000, 200, 5},     // 2ms脉冲,200ms间隔,5次
    {"长脉冲慢速",    5000, 500, 3},     // 5ms脉冲,500ms间隔,3次
};
```

**测试代码框架**:
```cpp
void RunPulseSequence(MultiCard& card, const TestCase& tc) {
    std::cout << "\n[TEST] " << tc.name << std::endl;
    std::cout << "脉冲宽度: " << tc.pulse_width_us << "μs, "
              << "间隔: " << tc.interval_ms << "ms, "
              << "次数: " << tc.repeat_count << std::endl;

    for (int i = 0; i < tc.repeat_count; i++) {
        std::cout << "  [" << (i+1) << "/" << tc.repeat_count << "] 发送脉冲...";

        int ret = card.MC_CmpPluse(2, 2, 0, tc.pulse_width_us, 0, 0, 0);
        if (ret != 0) {
            std::cout << " 失败: " << ret << std::endl;
            return;
        }
        std::cout << " 成功" << std::endl;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(tc.interval_ms)
        );
    }
}
```

**预期结果**:
- 所有脉冲发送成功(返回0)
- 电磁阀按指定频率开关
- 气流声音节奏与间隔一致

---

## 4. DO控制方式对比测试

**测试目标**: 对比CmpPluse和SetDoBit两种控制方式

**测试程序**: `test\test_y2_control_methods.cpp`

**测试参数**:
```cpp
// 方法1: MC_CmpPluse (脉冲方式)
// 方法2: MC_SetDoBit (电平方式)
short y2_bit_index = 1;  // Y2对应GPO bit 1 (Y1=bit0, Y2=bit1)
```

**测试代码框架**:
```cpp
// 方法1: CmpPluse方式
std::cout << "\n[方法1] CmpPluse脉冲控制" << std::endl;
int ret1 = card.MC_CmpPluse(2, 2, 0, 2000, 0, 0, 0);
std::cout << "返回值: " << ret1 << std::endl;
std::this_thread::sleep_for(std::chrono::seconds(1));

// 方法2: SetDoBit方式 (需手动控制时序)
std::cout << "\n[方法2] SetDoBit电平控制" << std::endl;

// 开启Y2
std::cout << "  设置Y2=高..." << std::endl;
int ret2 = card.MC_SetDoBit(MC_GPO, y2_bit_index, 1);
std::cout << "  返回值: " << ret2 << std::endl;

std::this_thread::sleep_for(std::chrono::milliseconds(2));

// 关闭Y2
std::cout << "  设置Y2=低..." << std::endl;
int ret3 = card.MC_SetDoBit(MC_GPO, y2_bit_index, 0);
std::cout << "  返回值: " << ret3 << std::endl;

// 验证状态
long do_value = 0;
card.MC_GetDo(MC_GPO, &do_value);
std::cout << "  当前GPO状态: 0x" << std::hex << do_value << std::dec << std::endl;
std::cout << "  Y2状态: " << ((do_value & (1 << y2_bit_index)) ? "高" : "低") << std::endl;
```

**预期结果**:
- 两种方式都能控制Y2
- CmpPluse自动时序,更适合点胶
- SetDoBit需要手动时序控制

---

## 5. 气压稳定性测试

**测试目标**: 验证连续操作下气压稳定性

**测试程序**: `test\test_y2_pressure_stability.cpp`

**测试参数**:
```cpp
const int stress_test_count = 100;       // 压力测试次数
const int pulse_width_us = 2000;         // 固定2ms脉冲
const int interval_ms = 50;              // 快速间隔50ms
```

**测试步骤**:
```cpp
1. 记录初始气压读数(如有气压传感器)
2. 连续发送100次脉冲
3. 记录每10次的气压变化
4. 分析压力波动范围
```

**预期结果**:
- 气压波动 < ±5% (47.5-52.5 kPa)
- 无压力持续下降趋势
- 无电磁阀过热现象

**失败诊断**:
- 压力下降: 检查气源/管路泄漏
- 压力波动大: 检查调压阀/缓冲罐
- 响应变慢: 电磁阀过热,增加间隔时间

---

## 6. 运动协调测试

**测试目标**: 验证Y2脉冲与轴运动同步

**测试程序**: `test\test_y2_coordinated_motion.cpp`

**测试场景**:
```cpp
// 场景1: 运动中触发Y2
// X轴移动100mm,中途50mm处触发Y2
double trigger_position = 50.0;  // 触发位置(mm)
double move_distance = 100.0;    // 总移动距离

// 场景2: 位置比较输出(CMP)
// 使用MC_CmpBufData实现精确位置触发
```

**测试代码框架**:
```cpp
// 初始化坐标系
short axes[] = {1, 2};
TestHelper::EnableAxes(card, axes, 2);
TestHelper::InitCoordSystem(card, 1, 1, 2);

// 开始运动
std::cout << "[INFO] X轴移动至100mm..." << std::endl;
card.MC_LnXY(1, 100.0 * 200, 0, 100.0, 2.0, 0, 0);

// 监控位置并触发
double current_pos = 0.0;
bool triggered = false;

while (current_pos < 100.0) {
    card.MC_GetAxisPrfPos(1, &current_pos);
    current_pos /= 200.0;  // 转换为mm

    if (!triggered && current_pos >= 50.0) {
        std::cout << "[TRIGGER] 位置: " << current_pos << "mm" << std::endl;
        card.MC_CmpPluse(2, 2, 0, 2000, 0, 0, 0);
        triggered = true;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```

**预期结果**:
- Y2在50mm±0.5mm范围内触发
- 运动不受脉冲影响
- 无抖动或失步

---

## 7. 异常处理测试

**测试目标**: 验证异常情况下的安全性

**测试场景**:

### 场景A: 连接中断
```cpp
// 发送脉冲过程中拔掉网线
1. 发送脉冲指令
2. 检查返回值
3. 预期: 返回-7 (超时) 或 -21 (断开)
```

### 场景B: 参数异常
```cpp
// 测试越界参数
int test_cases[][3] = {
    {2, 2, -100},      // 负脉冲宽度
    {2, 2, 1000000},   // 超长脉冲(1秒)
    {99, 2, 2000},     // 无效通道
};

for (auto& tc : test_cases) {
    int ret = card.MC_CmpPluse(tc[0], tc[1], 0, tc[2], 0, 0, 0);
    // 预期: 返回错误码,不崩溃
}
```

### 场景C: 紧急停止
```cpp
// 发送持续脉冲序列,中途按急停
1. 启动10次脉冲序列(间隔1s)
2. 第5次后按急停
3. 检查电磁阀立即停止
4. 预期: 剩余脉冲不执行
```

**预期结果**:
- 所有异常情况有明确错误码
- 无硬件损坏
- 可恢复正常操作

---

## 8. 性能基准测试

**测试目标**: 测量Y2控制的时序精度

**测试工具**: 示波器监测Y2输出

**测试指标**:

### 1. 脉冲宽度精度
- 设定: 2000μs (2ms)
- 测量: 实际脉冲宽度
- 容差: ±50μs (±2.5%)

### 2. 重复性
- 连续100次脉冲
- 测量标准差
- 要求: σ < 20μs

### 3. 最小脉冲宽度
- 测试范围: 100-1000μs
- 找到可靠工作的最小值
- 推荐: ≥500μs

### 4. 响应延迟
- API调用到电磁阀动作
- 测量: 示波器触发延迟
- 典型值: 1-3ms

---

## 9. 集成验证测试

**测试目标**: 完整点胶流程验证

**测试程序**: `test\test_y2_dispensing_workflow.cpp`

**测试流程**:
```cpp
1. 初始化系统
   - 连接控制卡
   - 使能轴
   - 回零

2. 执行点胶序列
   - 移动到点胶位置
   - 触发Y2脉冲(开启胶筒气压)
   - 等待点胶时间
   - 移动到下一位置

3. 重复N次
   - 测试5x5点阵
   - 间隔10mm

4. 验证结果
   - 所有点位置正确
   - 出胶量一致
   - 无遗漏点
```

**预期结果**:
- 25个点全部成功
- 位置精度 ±0.1mm
- 点胶时间一致性 < 5%

---

## 10. 文档和报告

**测试报告模板**:
```
Y2引脚控制电磁阀测试报告
=====================

测试日期: YYYY-MM-DD
测试人员:
硬件配置:
  - 控制卡型号:
  - 电磁阀型号:
  - 气源压力: 50kPa

测试结果汇总:
-------------
[√] 硬件连接验证
[√] 单次脉冲测试
[√] 多次脉冲测试
[√] 控制方式对比
[√] 气压稳定性测试
[√] 运动协调测试
[√] 异常处理测试
[√] 性能基准测试
[√] 集成验证测试

关键指标:
---------
- 脉冲宽度精度: 2000±30μs
- 响应延迟: 2.5ms
- 气压稳定性: ±3%
- 位置触发精度: ±0.3mm

问题与建议:
-----------
1. [问题] ...
   [建议] ...

2. [优化] ...
   [建议] ...

签字确认:
---------
测试: ______  日期: ______
审核: ______  日期: ______
```

---

## 构建和运行命令

```powershell
# 编译所有Y2测试
cmake -B build -S .
cmake --build build --config Release --target test_y2_single_pulse
cmake --build build --config Release --target test_y2_multiple_pulses
cmake --build build --config Release --target test_y2_control_methods
cmake --build build --config Release --target test_y2_coordinated_motion

# 运行测试
.\build\Release\test_y2_single_pulse.exe
.\build\Release\test_y2_multiple_pulses.exe
# ...依次运行

# 批量运行(需创建脚本)
.\scripts\run_y2_tests.bat
```

---

## 安全注意事项

### 测试前检查
- 确认气源压力在安全范围
- 检查所有接线牢固
- 准备紧急停止按钮

### 测试中监控
- 观察电磁阀温度
- 监听异常声音
- 检查气路泄漏

### 测试后验证
- 断开气源
- 关闭24V电源
- 记录测试数据

---

## API参考

### MC_CmpPluse函数原型
```cpp
int MC_CmpPluse(
    short nChannelMask,   // 通道掩码: 1=Y1, 2=Y2, 3=Y1+Y2
    short nPluseType1,    // 脉冲类型1: 0/1/2
    short nPluseType2,    // 脉冲类型2: 0/1/2
    short nTime1,         // 脉冲宽度1 (μs)
    short nTime2,         // 脉冲宽度2 (μs)
    short nTimeFlag1,     // 时间标志1
    short nTimeFlag2      // 时间标志2
);
```

### MC_SetDoBit函数原型
```cpp
int MC_SetDoBit(
    short nDoType,   // 输出类型: MC_GPO=12
    short nDoNum,    // 输出编号: 0=Y1, 1=Y2
    short value      // 输出值: 0=低, 1=高
);
```

### MC_GetDo函数原型
```cpp
int MC_GetDo(
    short nDoType,   // 输出类型: MC_GPO=12
    long *pValue     // 返回值指针
);
```

---

## 相关配置文件

### config/machine_config.ini
```ini
[Dispensing]
pressure=50.0
dispensing_time=0.1

[ValveTiming]
open_delay=0.001
close_delay=0.001
min_on_time=0.01

[CMP]
cmp_channel=1
pulse_width_us=2000
```

---

## 参考文档

- `test\test_CmpPluse.cpp` - 基础脉冲测试实现
- `src\hardware\MultiCardCPP.h` - MultiCard API参考
- `src\hardware\CLAUDE.md` - 硬件模块约束
- `docs\error-codes.md` - 错误代码参考
- `config\machine_config.ini` - 系统配置

---

**文档版本**: 1.0
**创建日期**: 2025-12-02
**测试环境**: Windows + GAS运动控制卡
**基准实现**: test\test_CmpPluse.cpp
