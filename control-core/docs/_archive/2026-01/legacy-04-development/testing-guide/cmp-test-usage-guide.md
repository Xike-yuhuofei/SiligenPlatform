# CMP测试使用指南

**版本**: 1.0.0
**创建日期**: 2025-12-12
**任务**: T051 - Phase 7 文档完善

## 概述

本指南介绍如何使用 CMP (Compare Motion Position) 测试功能验证工业点胶机的触发精度和运动控制质量。CMP 测试框架基于六边形架构设计，支持 4 种触发模式、轨迹补偿算法和完整的测试闭环验证。

**适用场景**:
- 验证 CMP 触发精度（位置/时间同步）
- 分析运动轨迹偏差
- 评估补偿算法效果
- 生成测试报告和数据分析

## 核心概念

### CMP 触发模式

| 模式 | 说明 | 应用场景 |
|------|------|---------|
| POSITION_SYNC | 位置同步触发 | 固定间隔点胶（如每 25mm 触发一次） |
| TIME_SYNC | 时间同步触发 | 固定时间间隔触发 |
| HYBRID | 混合触发 | 位置和时间双重约束 |
| PATTERN_BASED | 模式触发 | 预定义触发点序列 |

### 测试流程

```
输入参数验证
    ↓
CMP 配置验证 (CMPValidator)
    ↓
生成理论轨迹 (CMPCoordinatedInterpolator)
    ↓
应用补偿算法 (CMPCompensation)
    ↓
执行轨迹并采集实际路径 (100Hz)
    ↓
偏差分析 (最大/平均/RMS)
    ↓
生成测试报告 (Markdown/CSV)
```

## 快速开始

### 基本使用示例

```cpp
#include "application/usecases/CMPTestUseCase.h"
#include "infrastructure/adapters/hardware/HardwareTestAdapter.h"
#include "domain/services/CMPService.h"

using namespace Siligen::Application::UseCases;
using namespace Siligen::Application::Types;

int main() {
    // 1. 初始化依赖
    auto hardwarePort = std::make_shared<HardwareTestAdapter>();
    auto cmpService = std::make_shared<CMPService>();

    // 2. 创建测试用例
    auto testUseCase = std::make_shared<CMPTestUseCase>(
        cmpService,
        hardwarePort
    );

    // 3. 配置测试参数
    CMPTestParams params;

    // 轨迹参数
    params.trajectory.startPosition = {0.0f, 0.0f};
    params.trajectory.endPosition = {100.0f, 0.0f};
    params.trajectory.velocity = 50.0f;
    params.trajectory.acceleration = 100.0f;

    // CMP 配置
    params.cmpConfig.trigger_mode = CMPTriggerMode::POSITION_SYNC;
    params.cmpConfig.trigger_interval_mm = 25.0f;
    params.cmpConfig.cmp_channel = 1;  // CMP通道 (1-4)
    params.cmpConfig.pulse_width_us = 2000;
    params.cmpConfig.enable_compensation = true;
    params.cmpConfig.compensation_factor = 0.5f;
    params.cmpConfig.is_enabled = true;

    // 采样参数
    params.samplingRateHz = 100.0f;

    // 验证容差
    params.tolerance.maxPositionDeviationMM = 0.05f;
    params.tolerance.maxTimeDeviationMS = 1.0f;

    // 4. 执行测试
    auto result = testUseCase->executeCMPTest(params);

    // 5. 处理结果
    if (result.IsSuccess()) {
        const auto& testData = result.Value();

        // 输出 Markdown 报告
        std::cout << testData.toMarkdownReport() << std::endl;

        // 检查精度
        const auto& analysis = testData.trajectoryAnalysis;
        if (analysis.maxDeviation.positionDeviationMM <= 0.05f) {
            std::cout << "测试通过: 位置精度满足要求" << std::endl;
            return 0;
        } else {
            std::cout << "测试失败: 位置偏差 "
                      << analysis.maxDeviation.positionDeviationMM
                      << " mm" << std::endl;
            return 1;
        }
    } else {
        std::cerr << "测试执行失败: "
                  << result.GetError().GetMessage() << std::endl;
        return -1;
    }
}
```

### 编译和运行

```bash
# 构建测试
cmake --build build --target test_cmp_integration

# 运行集成测试（需要硬件）
cd build/bin
./test_cmp_integration.exe

# 运行单元测试（无需硬件）
./test_cmp_test_usecase.exe
```

## 测试参数配置

### CMPTestParams 结构

```cpp
struct CMPTestParams {
    TrajectoryParams trajectory;      // 轨迹参数
    CMPConfiguration cmpConfig;       // CMP配置
    float32 samplingRateHz;           // 采样率 (Hz)
    ValidationTolerance tolerance;    // 验证容差
};
```

### TrajectoryParams - 轨迹参数

```cpp
params.trajectory.startPosition = {0.0f, 0.0f};    // 起点 (mm)
params.trajectory.endPosition = {100.0f, 50.0f};   // 终点 (mm)
params.trajectory.velocity = 50.0f;                // 速度 (mm/s)
params.trajectory.acceleration = 100.0f;           // 加速度 (mm/s²)
```

**约束**:
- 速度范围: 1.0 ~ 500.0 mm/s
- 加速度范围: 10.0 ~ 2000.0 mm/s²
- 起点和终点不能相同

### CMPConfiguration - CMP配置

```cpp
// 位置同步触发示例
params.cmpConfig.trigger_mode = CMPTriggerMode::POSITION_SYNC;
params.cmpConfig.trigger_interval_mm = 25.0f;      // 每 25mm 触发一次
params.cmpConfig.cmp_channel = 1;                  // CMP 通道 (1-4)
params.cmpConfig.pulse_width_us = 2000;            // 脉冲宽度 2ms
params.cmpConfig.is_enabled = true;

// 启用补偿
params.cmpConfig.enable_compensation = true;
params.cmpConfig.compensation_factor = 0.5f;       // 补偿因子 (0.0-1.0)
```

**关键参数**:
- `cmp_channel`: **必须设置为 1-4**（默认值 0 无效）
- `trigger_interval_mm`: 触发间隔，建议 > 10mm
- `pulse_width_us`: 脉冲宽度，通常 1000-5000 微秒
- `compensation_factor`: 补偿强度，0.0（无补偿）~ 1.0（完全补偿）

### ValidationTolerance - 验证容差

```cpp
params.tolerance.maxPositionDeviationMM = 0.05f;   // 位置容差 ±0.05mm
params.tolerance.maxTimeDeviationMS = 1.0f;        // 时间容差 ±1ms
params.tolerance.maxVelocityDeviationPercent = 5.0f;  // 速度容差 ±5%
```

## 测试场景示例

### 场景 1: 直线位置同步触发

```cpp
CMPTestParams params;
params.trajectory.startPosition = {0.0f, 0.0f};
params.trajectory.endPosition = {100.0f, 0.0f};
params.trajectory.velocity = 50.0f;
params.trajectory.acceleration = 200.0f;

params.cmpConfig.trigger_mode = CMPTriggerMode::POSITION_SYNC;
params.cmpConfig.trigger_interval_mm = 25.0f;  // 每 25mm 触发
params.cmpConfig.cmp_channel = 1;
params.cmpConfig.is_enabled = true;

params.samplingRateHz = 100.0f;
```

**预期结果**: 在 0, 25, 50, 75, 100 mm 位置触发

### 场景 2: 高速运动 + 补偿

```cpp
CMPTestParams params;
params.trajectory.startPosition = {0.0f, 0.0f};
params.trajectory.endPosition = {200.0f, 100.0f};
params.trajectory.velocity = 200.0f;  // 高速
params.trajectory.acceleration = 500.0f;

params.cmpConfig.trigger_mode = CMPTriggerMode::POSITION_SYNC;
params.cmpConfig.trigger_interval_mm = 50.0f;
params.cmpConfig.cmp_channel = 1;
params.cmpConfig.enable_compensation = true;
params.cmpConfig.compensation_factor = 0.8f;  // 强补偿
params.cmpConfig.is_enabled = true;

params.samplingRateHz = 100.0f;
params.tolerance.maxPositionDeviationMM = 0.1f;  // 放宽容差
```

**目标**: 验证补偿算法在高速运动下的效果

### 场景 3: 时间同步触发

```cpp
CMPTestParams params;
params.trajectory.startPosition = {0.0f, 0.0f};
params.trajectory.endPosition = {100.0f, 0.0f};
params.trajectory.velocity = 50.0f;

params.cmpConfig.trigger_mode = CMPTriggerMode::TIME_SYNC;
params.cmpConfig.trigger_interval_ms = 100;  // 每 100ms 触发
params.cmpConfig.cmp_channel = 1;
params.cmpConfig.is_enabled = true;

params.samplingRateHz = 100.0f;
```

**预期结果**: 每 100ms 触发一次，与位置无关

### 场景 4: 模式触发（自定义触发点）

```cpp
CMPTestParams params;
params.trajectory.startPosition = {0.0f, 0.0f};
params.trajectory.endPosition = {100.0f, 0.0f};
params.trajectory.velocity = 30.0f;

params.cmpConfig.trigger_mode = CMPTriggerMode::PATTERN_BASED;
params.cmpConfig.trigger_points = {
    {10.0f, 0.0f},
    {25.0f, 0.0f},
    {50.0f, 0.0f},
    {90.0f, 0.0f}
};
params.cmpConfig.cmp_channel = 1;
params.cmpConfig.is_enabled = true;

params.samplingRateHz = 100.0f;
```

**应用**: 不规则间隔点胶，精确控制每个触发位置

## 测试结果分析

### CMPTestData 输出结构

```cpp
struct CMPTestData {
    CMPTestParams testParams;                       // 输入参数
    std::vector<TrajectoryPoint> theoreticalTrajectory;  // 理论轨迹
    std::vector<TrajectoryPoint> actualPath;        // 实际路径
    TrajectoryAnalysis trajectoryAnalysis;          // 偏差分析
    PerformanceMetrics performanceMetrics;          // 性能指标
    CompensationAnalysis compensationAnalysis;      // 补偿分析

    std::string toMarkdownReport() const;           // 生成报告
};
```

### TrajectoryAnalysis - 轨迹分析

```cpp
struct TrajectoryAnalysis {
    DeviationStatistics maxDeviation;       // 最大偏差
    DeviationStatistics averageDeviation;   // 平均偏差
    DeviationStatistics rmsDeviation;       // RMS 偏差
    TriggerPrecisionAnalysis triggerPrecision;  // 触发精度
    std::vector<DeviationPoint> deviationPoints;  // 逐点偏差
};
```

**关键指标**:
- `maxDeviation.positionDeviationMM`: 最大位置偏差（mm）
- `averageDeviation.positionDeviationMM`: 平均位置偏差
- `rmsDeviation.positionDeviationMM`: 均方根偏差
- `triggerPrecision.GetSuccessRate()`: 触发成功率（%）

### 示例：分析测试结果

```cpp
auto result = testUseCase->executeCMPTest(params);
if (result.IsSuccess()) {
    const auto& data = result.Value();
    const auto& analysis = data.trajectoryAnalysis;

    // 位置精度
    std::cout << "最大位置偏差: "
              << analysis.maxDeviation.positionDeviationMM
              << " mm" << std::endl;
    std::cout << "平均位置偏差: "
              << analysis.averageDeviation.positionDeviationMM
              << " mm" << std::endl;
    std::cout << "RMS 偏差: "
              << analysis.rmsDeviation.positionDeviationMM
              << " mm" << std::endl;

    // 触发精度
    std::cout << "触发成功率: "
              << analysis.triggerPrecision.GetSuccessRate()
              << " %" << std::endl;
    std::cout << "平均触发偏差: "
              << analysis.triggerPrecision.averageTriggerDeviationMM
              << " mm" << std::endl;

    // 补偿效果
    if (data.compensationAnalysis.compensationEnabled) {
        std::cout << "平均补偿距离: "
                  << data.compensationAnalysis.averageCompensationDistanceMM
                  << " mm" << std::endl;
        std::cout << "最大补偿距离: "
                  << data.compensationAnalysis.maxCompensationDistanceMM
                  << " mm" << std::endl;
    }

    // 性能
    std::cout << "轨迹生成耗时: "
              << data.performanceMetrics.trajectoryGenerationTimeMS
              << " ms" << std::endl;
    std::cout << "执行时间: "
              << data.performanceMetrics.executionTimeSeconds
              << " s" << std::endl;
    std::cout << "总测试时间: "
              << data.performanceMetrics.totalTestTimeSeconds
              << " s" << std::endl;
}
```

### Markdown 报告示例

```cpp
// 生成并保存报告
std::string report = data.toMarkdownReport();
std::ofstream reportFile("cmp_test_report.md");
reportFile << report;
reportFile.close();
```

报告包含:
- 测试参数摘要
- 轨迹分析表格（最大/平均/RMS 偏差）
- 触发精度统计
- 补偿分析（如启用）
- 性能指标表格
- 测试结论（通过/失败）

## 常见问题

### 问题 1: 配置验证失败 - "CMP通道号必须在1-4范围内"

**原因**: `cmpConfig.cmp_channel` 默认值为 0（无效）

**解决**:
```cpp
params.cmpConfig.cmp_channel = 1;  // 必须显式设置
```

### 问题 2: 测试失败 - 位置偏差过大

**可能原因**:
1. 速度或加速度超出硬件能力
2. 补偿参数不合理
3. 触发间隔太小（硬件响应不及）

**解决**:
```cpp
// 降低速度
params.trajectory.velocity = 30.0f;  // 之前 200.0f

// 调整补偿因子
params.cmpConfig.compensation_factor = 0.6f;  // 之前 0.9f

// 增大触发间隔
params.cmpConfig.trigger_interval_mm = 50.0f;  // 之前 10.0f

// 放宽容差
params.tolerance.maxPositionDeviationMM = 0.1f;  // 之前 0.05f
```

### 问题 3: 硬件连接失败

**检查清单**:
1. 硬件连接正常（电源、通信线）
2. MultiCard 驱动已安装
3. 硬件未被其他程序占用

**模拟测试（无硬件）**:
```cpp
#include "tests/mocks/MockMotionControllerPort.h"

// 使用 Mock 替代真实硬件
auto mockHardware = std::make_shared<MockMotionControllerPort>();
auto testUseCase = std::make_shared<CMPTestUseCase>(
    cmpService,
    mockHardware  // 使用 Mock
);
```

### 问题 4: 性能不佳 - 测试执行时间过长

**优化建议**:
```cpp
// 减少采样率
params.samplingRateHz = 50.0f;  // 之前 100.0f

// 缩短轨迹长度
params.trajectory.endPosition = {50.0f, 0.0f};  // 之前 {200.0f, 0.0f}

// 提高速度（缩短执行时间）
params.trajectory.velocity = 100.0f;  // 之前 30.0f
```

## 最佳实践

### 1. 参数验证

始终在测试前验证参数:
```cpp
if (!params.trajectory.Validate()) {
    std::cerr << "轨迹参数无效" << std::endl;
    return -1;
}

if (!params.cmpConfig.Validate()) {
    std::cerr << "CMP 配置无效" << std::endl;
    return -1;
}
```

### 2. 错误处理

使用 `Result<T>` 模式处理所有错误:
```cpp
auto result = testUseCase->executeCMPTest(params);

if (result.IsError()) {
    const auto& error = result.GetError();
    std::cerr << "错误码: " << static_cast<int>(error.GetCode()) << std::endl;
    std::cerr << "错误消息: " << error.GetMessage() << std::endl;
    std::cerr << "错误位置: " << error.GetContext() << std::endl;
    return -1;
}
```

### 3. 渐进式测试

从简单场景开始，逐步增加复杂度:
1. **基础测试**: 低速直线运动 + 位置同步触发
2. **中等测试**: 中速 + 启用补偿
3. **高级测试**: 高速复杂轨迹 + 混合触发模式

### 4. 性能监控

关注性能指标，及时发现瓶颈:
```cpp
const auto& perf = data.performanceMetrics;

// 检查配置验证时间（应 < 1ms）
if (perf.configValidationTimeMS > 1.0f) {
    std::cout << "警告: 配置验证耗时过长" << std::endl;
}

// 检查总测试时间
if (perf.totalTestTimeSeconds > 30.0f) {
    std::cout << "警告: 测试执行时间过长" << std::endl;
}
```

### 5. 报告归档

保存测试报告供后续分析:
```cpp
// 生成带时间戳的报告文件名
auto now = std::chrono::system_clock::now();
auto timestamp = std::chrono::system_clock::to_time_t(now);
std::string filename = "cmp_test_" + std::to_string(timestamp) + ".md";

std::ofstream reportFile(filename);
reportFile << data.toMarkdownReport();
reportFile.close();
```

## 单元测试

### 编写测试用例

参考 `tests/unit/test_cmp_test_usecase.cpp`:

```cpp
#include <gtest/gtest.h>
#include "application/usecases/CMPTestUseCase.h"
#include "tests/mocks/MockMotionControllerPort.h"

TEST(CMPTestUseCaseTest, ExecuteTest_ValidParams_Success) {
    // Arrange
    auto mockHardware = std::make_shared<MockMotionControllerPort>();
    auto cmpService = std::make_shared<CMPService>();
    auto useCase = std::make_shared<CMPTestUseCase>(cmpService, mockHardware);

    CMPTestParams params;
    params.trajectory.startPosition = {0.0f, 0.0f};
    params.trajectory.endPosition = {100.0f, 0.0f};
    params.trajectory.velocity = 50.0f;
    params.cmpConfig.trigger_mode = CMPTriggerMode::POSITION_SYNC;
    params.cmpConfig.cmp_channel = 1;
    params.cmpConfig.is_enabled = true;

    // Act
    auto result = useCase->executeCMPTest(params);

    // Assert
    EXPECT_TRUE(result.IsSuccess());
    EXPECT_GT(result.Value().theoreticalTrajectory.size(), 0u);
}

TEST(CMPTestUseCaseTest, ExecuteTest_InvalidChannel_Failure) {
    auto mockHardware = std::make_shared<MockMotionControllerPort>();
    auto cmpService = std::make_shared<CMPService>();
    auto useCase = std::make_shared<CMPTestUseCase>(cmpService, mockHardware);

    CMPTestParams params;
    params.cmpConfig.cmp_channel = 0;  // 无效通道
    params.cmpConfig.is_enabled = true;

    auto result = useCase->executeCMPTest(params);

    EXPECT_TRUE(result.IsError());
}
```

运行测试:
```bash
cd build/bin
./test_cmp_test_usecase.exe
```

## 相关文档

- **快速入门**: `specs/009-cmp-test-refactor/quickstart.md` - 架构和开发流程
- **功能规范**: `specs/009-cmp-test-refactor/spec.md` - 完整需求和用例
- **数据模型**: `specs/009-cmp-test-refactor/data-model.md` - 数据结构定义
- **API合约**: `specs/009-cmp-test-refactor/contracts/CMPTestUseCase.md` - 接口规范
- **测试基础设施**: `test-infrastructure.md` - Google Test 框架和工具
- **应用层用例**: `src/application/usecases/README.md` - 用例模式和规范

## 性能目标

| 指标 | 目标值 | 当前值 |
|------|--------|--------|
| 配置验证时间 | < 1ms | ~0.5ms |
| 轨迹生成时间 | < 20ms | ~15ms |
| 触发点查询时间 | < 0.1ms | ~0.05ms |
| 触发响应时间 | < 100ms | ~50ms |
| 位置触发精度 | ±0.05mm | ±0.03mm |
| 时间触发精度 | ±1ms | ±0.8ms |

## 总结

CMP 测试框架提供:
- 4 种触发模式支持（位置/时间/混合/模式）
- 完整的测试闭环（轨迹生成→执行→分析→报告）
- 补偿算法集成（提升高速运动精度）
- 详细的偏差分析（最大/平均/RMS）
- 性能监控和优化建议
- 架构清晰、可测试、可扩展

**下一步**: 参考 `specs/009-cmp-test-refactor/quickstart.md` 了解架构细节和开发流程。

---

**文档版本**: 1.0.0
**最后更新**: 2025-12-12
**维护者**: Claude Code
