# 测试用例架构问题分析

**发现日期**: 2026-01-16
**严重程度**: 🔴 高 (架构违反)
**影响范围**: 所有 `*TestUseCase`

---

> 更新 (2026-01-17):
> - `*TestUseCase` 已迁移到 `tests/unit/usecases`，不再属于 Application 层
> - `HomeAxesUseCase` 已实现并可用
> - `IHomingPort` 已具备基础适配器实现

## 🎯 核心问题

### 问题陈述

**当前架构**: 测试用例 (`*TestUseCase`) 直接调用 `IHardwareTestPort`,绕过了生产级的业务逻辑。

**应该是**: 测试用例应该**调用生产用例/Domain服务**,验证生产代码路径,而不是重新实现一套测试专用逻辑。

---

## 📊 问题对比

### 当前错误架构

```
┌─────────────────────────────────────────────────────────┐
│                  Application Layer                       │
├──────────────────────────┬──────────────────────────────┤
│   Production UseCase     │    Test UseCase (❌)         │
│                          │                              │
│  HomeAxesUseCase         │  HomingTestUseCase           │
│  (已实现)                │  (直接调用硬件层)             │
│                          │                              │
│  [应该存在但缺失]         │  executeHomingTest() {       │
│                          │    ❌ m_hardwarePort->       │
│                          │         startHoming(...)     │
│                          │    ❌ pollHomingStatus(...)  │
│                          │    ❌ 重复实现业务逻辑       │
│                          │  }                           │
└──────────────────────────┴──────────────────────────────┘
                  ↓                      ↓
┌─────────────────────────────────────────────────────────┐
│                   Domain Layer                           │
│                                                          │
│  HomingController (✅)        [被测试用例绕过!]          │
│    - CheckHomingSafety()                                │
│    - ValidateHomingParameters()                         │
│    - StartHoming()                                      │
│    - WaitForHomingComplete()                            │
└─────────────────────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────────────────────┐
│              Infrastructure Layer                        │
│                                                          │
│  IHardwareTestPort (Adapter)                            │
└─────────────────────────────────────────────────────────┘
```

### 正确的架构

```
┌─────────────────────────────────────────────────────────┐
│                  Application Layer                       │
├──────────────────────────┬──────────────────────────────┤
│   Production UseCase     │    Test UseCase (✅)         │
│                          │                              │
│  HomeAxesUseCase         │  HomingTestUseCase           │
│    Execute() {           │    executeHomingTest() {     │
│      ✅ 调用Domain服务   │      ✅ 调用 HomeAxesUseCase │
│      ✅ 业务流程编排     │      ✅ 记录测试数据         │
│    }                     │      ✅ 保存测试记录         │
│                          │    }                         │
└──────────────────────────┴──────────────────────────────┘
                  ↓                      ↓
                  └──────────┬───────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│                   Domain Layer                           │
│                                                          │
│  HomingController (✅)        [生产和测试共用!]          │
│    - CheckHomingSafety()                                │
│    - ValidateHomingParameters()                         │
│    - StartHoming()                                      │
│    - WaitForHomingComplete()                            │
└─────────────────────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────────────────────┐
│              Infrastructure Layer                        │
│                                                          │
│  IHomingPort (Adapter)                                  │
└─────────────────────────────────────────────────────────┘
```

---

## 🔍 代码证据

### 证据1: Domain层已有生产级服务

#### `HomingController` (Domain层)
```cpp
// src/domain/services/HomingController.h
class HomingController {
public:
    Result<void> StartHoming(short axis, const HomingParameters& params) noexcept;
    Result<void> StopHoming(short axis) noexcept;
    Result<HomingState> GetHomingState(short axis) const noexcept;
    Result<bool> IsAxisHomed(short axis) const noexcept;
    Result<void> WaitForHomingComplete(short axis, int32_t timeout_ms = 30000) noexcept;

private:
    Result<void> CheckHomingSafety(short axis) const noexcept;
    Result<void> ValidateHomingParameters(const HomingParameters& params) const noexcept;
};
```

#### `JogController` (Domain层)
```cpp
// src/domain/services/JogController.h
class JogController {
public:
    Result<void> StartContinuousJog(short axis, int16_t direction, float velocity) noexcept;
    Result<void> StartStepJog(short axis, int16_t direction, float distance, float velocity) noexcept;
    Result<void> StopJog(short axis) noexcept;
    Result<JogState> GetJogState(short axis) const noexcept;

private:
    Result<void> CheckSafetyInterlocks(short axis, int16_t direction) const noexcept;
    Result<void> ValidateJogParameters(short axis, float velocity) const noexcept;
};
```

### 证据2: 测试用例重复实现业务逻辑

#### `HomingTestUseCase` (Application层 - ❌ 错误)
```cpp
// tests/unit/usecases/HomingTestUseCase.cpp
Result<HomingTestData> HomingTestUseCase::executeHomingTest(const HomingTestParams& params) {
    // ❌ 直接调用硬件层,绕过Domain层
    auto startResult = m_hardwarePort->startHoming(
        params.axes, params.mode, params.searchSpeed, params.returnSpeed, params.direction);

    // ❌ 重复实现轮询逻辑
    bool allSucceeded = pollHomingStatus(params.axes, params.timeoutMs, axisResults);

    // ❌ 重复实现参数验证
    Result<void> HomingTestUseCase::validateParameters(const HomingTestParams& params) {
        if (params.axes.empty()) { return Failure(...); }
        if (params.searchSpeed <= 0) { return Failure(...); }
        // ...
    }
}
```

#### `JogTestUseCase` (Application层 - ❌ 错误)
```cpp
// tests/unit/usecases/JogTestUseCase.cpp
Result<JogTestData> JogTestUseCase::executeJogTest(const JogTestParams& params) {
    // ❌ 直接调用硬件层
    auto currentPosResult = m_hardwarePort->getAxisPosition(params.axis);

    // ❌ 重复实现安全检查
    auto safetyResult = checkSafetyLimits(params, startPosition);

    // ❌ 重复实现参数验证
    Result<void> JogTestUseCase::validateParameters(const JogTestParams& params) const {
        if (params.axis < 0 || params.axis > 3) { return Failure(...); }
        if (params.speed <= 0) { return Failure(...); }
        // ...
    }
}
```

---

## ❌ 架构违反清单

### 违反1: 重复业务逻辑 (DRY原则)

| Domain层 (生产) | TestUseCase层 (测试) | 重复内容 |
|----------------|---------------------|---------|
| `HomingController::ValidateHomingParameters()` | `HomingTestUseCase::validateParameters()` | 参数验证逻辑 |
| `HomingController::CheckHomingSafety()` | `HomingTestUseCase::executeHomingTest()` | 安全检查逻辑 |
| `HomingController::WaitForHomingComplete()` | `HomingTestUseCase::pollHomingStatus()` | 轮询等待逻辑 |
| `JogController::ValidateJogParameters()` | `JogTestUseCase::validateParameters()` | 参数验证逻辑 |
| `JogController::CheckSafetyInterlocks()` | `JogTestUseCase::checkSafetyLimits()` | 安全互锁逻辑 |

### 违反2: 测试覆盖不完整

**问题**: 测试用例绕过了生产代码路径,无法验证以下内容:
- ❌ `HomingController` 的业务逻辑是否正确
- ❌ `JogController` 的业务逻辑是否正确
- ❌ Application层用例的编排逻辑是否正确
- ❌ Domain层的安全检查是否有效

**结果**: 测试通过 ≠ 生产代码正确

### 违反3: 缺失生产级Application用例

**发现**: 以下生产用例应该存在但实际缺失:

| Domain服务 | 应有的Application用例 | 当前状态 |
|-----------|---------------------|---------|
| `HomingController` | `HomeAxesUseCase` (存在头文件) | ✅ 有头文件定义 |
| `JogController` | `JogUseCase` 或 `ManualMotionControlUseCase` | ❓ 可能通过 `ManualMotionControlUseCase` 实现 |

---

## ✅ 正确的实现方案

### 方案1: Application用例调用Domain服务

#### Step 1: 确保生产用例存在并实现

```cpp
// src/application/usecases/HomeAxesUseCase.h
class HomeAxesUseCase {
public:
    explicit HomeAxesUseCase(
        std::shared_ptr<Domain::Services::HomingController> homing_controller,
        std::shared_ptr<Domain::Ports::IEventPublisherPort> event_port);

    /**
     * @brief 执行回零操作
     */
    Result<HomeAxesResponse> Execute(const HomeAxesRequest& request);

private:
    std::shared_ptr<Domain::Services::HomingController> homing_controller_;
    std::shared_ptr<Domain::Ports::IEventPublisherPort> event_port_;

    // 应用层职责: 流程编排
    Result<void> ValidateRequest(const HomeAxesRequest& request);
    Result<void> HomeAxis(short axis, const HomingParameters& params, HomeAxesResponse& response);
    void PublishHomingEvent(short axis, bool success);
};
```

#### Step 2: 测试用例调用生产用例

```cpp
// src/application/usecases/HomingTestUseCase.h (重构后)
class HomingTestUseCase {
public:
    explicit HomingTestUseCase(
        std::shared_ptr<HomeAxesUseCase> home_axes_usecase,  // ✅ 依赖生产用例
        ITestRecordRepository* repository);

    /**
     * @brief 执行回零测试
     * ✅ 测试用例职责: 调用生产用例 + 记录测试数据
     */
    Result<HomingTestData> executeHomingTest(const HomingTestParams& params);

private:
    std::shared_ptr<HomeAxesUseCase> home_axes_usecase_;  // ✅ 重用生产逻辑
    ITestRecordRepository* repository_;

    // ✅ 测试用例仅负责测试特定逻辑
    HomingTestData ConvertToTestData(const HomeAxesResponse& response);
    Result<int64_t> SaveTestRecord(const HomingTestData& data);
};
```

```cpp
// src/application/usecases/HomingTestUseCase.cpp (重构后)
Result<HomingTestData> HomingTestUseCase::executeHomingTest(const HomingTestParams& params) {
    // ✅ 转换测试参数到生产用例参数
    HomeAxesRequest request;
    request.axes = params.axes;
    request.home_all_axes = false;
    request.wait_for_completion = true;
    request.timeout_ms = params.timeoutMs;

    // ✅ 调用生产用例 (复用所有业务逻辑)
    auto result = home_axes_usecase_->Execute(request);

    if (result.IsError()) {
        return Result<HomingTestData>::Failure(result.GetError());
    }

    // ✅ 测试用例的增值: 转换为测试数据格式
    HomingTestData testData = ConvertToTestData(result.Value());

    // ✅ 测试用例的增值: 保存测试记录
    SaveTestRecord(testData);

    return Result<HomingTestData>::Success(testData);
}
```

---

### 方案2: 测试用例直接调用Domain服务 (简化方案)

如果生产用例实现成本高,可以让测试用例直接调用Domain服务:

```cpp
// src/application/usecases/HomingTestUseCase.h (简化方案)
class HomingTestUseCase {
public:
    explicit HomingTestUseCase(
        std::shared_ptr<Domain::Services::HomingController> homing_controller,  // ✅ 依赖Domain服务
        ITestRecordRepository* repository);

    Result<HomingTestData> executeHomingTest(const HomingTestParams& params);

private:
    std::shared_ptr<Domain::Services::HomingController> homing_controller_;  // ✅ 重用Domain逻辑
    ITestRecordRepository* repository_;
};
```

```cpp
// src/application/usecases/HomingTestUseCase.cpp (简化方案)
Result<HomingTestData> HomingTestUseCase::executeHomingTest(const HomingTestParams& params) {
    HomingTestData testData;

    // ✅ 调用Domain服务 (复用业务逻辑)
    for (int axis : params.axes) {
        HomingParameters homingParams;
        homingParams.search_speed = params.searchSpeed;
        homingParams.return_speed = params.returnSpeed;
        // ...

        // ✅ 复用Domain层的所有逻辑
        auto result = homing_controller_->StartHoming(axis, homingParams);
        if (result.IsError()) {
            testData.axisResults[axis].success = false;
            testData.axisResults[axis].failureReason = result.GetError().GetMessage();
            continue;
        }

        // ✅ 复用Domain层的等待逻辑
        auto waitResult = homing_controller_->WaitForHomingComplete(axis, params.timeoutMs);
        testData.axisResults[axis].success = waitResult.IsSuccess();
    }

    // ✅ 测试用例的增值: 保存测试记录
    SaveTestRecord(testData);

    return Result<HomingTestData>::Success(testData);
}
```

---

## 🎯 架构原则总结

### ★ Insight ─────────────────────────────────────

**原则1: 测试用例不应该重新实现业务逻辑**
- 测试用例应该调用生产代码路径
- 测试用例的增值在于: 测试数据记录、可视化、报告生成

**原则2: 分层职责明确**
- **Domain层**: 业务逻辑 (安全检查、参数验证)
- **Application层**: 流程编排 (调用Domain服务)
- **TestUseCase层**: 测试编排 (调用生产用例 + 测试数据记录)

**原则3: DRY原则**
- 参数验证逻辑 → Domain层
- 安全检查逻辑 → Domain层
- 等待完成逻辑 → Domain层
- 测试数据记录 → TestUseCase层

─────────────────────────────────────────────────

---

## 📋 重构检查清单

### Phase 1: 确认生产用例存在

- [x] `HomeAxesUseCase` - 已实现
- [ ] `JogUseCase` 或确认 `ManualMotionControlUseCase` 包含Jog功能
- [ ] 其他Domain服务是否有对应的Application用例

### Phase 2: 重构测试用例

#### 高优先级 (已有Domain服务)
- [ ] `HomingTestUseCase` → 调用 `HomeAxesUseCase` 或 `HomingController`
- [ ] `JogTestUseCase` → 调用 `JogUseCase` 或 `JogController`

#### 中优先级
- [ ] `InterpolationTestUseCase` → 调用对应的生产用例
- [ ] `TriggerTestUseCase` → 调用对应的生产用例
- [ ] `IOTestUseCase` → 调用对应的生产用例
- [ ] `CMPTestUseCase` → 调用对应的生产用例

#### 低优先级 (已是高层用例)
- [ ] `DiagnosticTestUseCase` - 保持现状 (它本身就是编排多个测试)

### Phase 3: 删除重复代码

- [ ] 删除测试用例中的参数验证逻辑 (改为调用Domain层)
- [ ] 删除测试用例中的安全检查逻辑 (改为调用Domain层)
- [ ] 删除测试用例中的等待完成逻辑 (改为调用Domain层)

---

## 📊 影响评估

### 收益

| 指标 | 当前 | 重构后 | 改善 |
|------|------|--------|------|
| 代码重复 | ~1000行重复逻辑 | 0行 | ⬇️ 100% |
| 测试覆盖 | 仅测试硬件层 | 测试完整业务路径 | ⬆️ 显著提升 |
| 维护成本 | 修改需要改2处 | 修改仅需改1处 | ⬇️ 50% |
| 架构一致性 | 违反分层原则 | 符合六边形架构 | ✅ |

### 风险

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 重构工作量大 | 🟡 中等 | 逐个用例重构,不影响现有功能 |
| 引入新Bug | 🟢 低 | 单元测试覆盖,逐步迁移 |
| 测试数据格式变化 | 🟢 低 | 保持测试数据格式不变 |

### 时间估算

| 任务 | 预估时间 |
|------|---------|
| Phase 1: 确认生产用例 | 1天 |
| Phase 2: 重构测试用例 (6个) | 3-4天 |
| Phase 3: 删除重复代码 | 1天 |
| **总计** | **5-6天** |

---

## 🎯 结论

**当前测试用例架构存在严重问题**:
1. ❌ 绕过生产代码路径,无法验证生产逻辑
2. ❌ 重复实现业务逻辑,违反DRY原则
3. ❌ 违反分层架构原则

**正确做法**:
1. ✅ 测试用例调用生产用例/Domain服务
2. ✅ 测试用例仅负责测试数据记录
3. ✅ 确保测试覆盖生产代码路径

---

**审阅者**: 请确认
- [ ] 是否同意当前架构存在问题
- [ ] 是否同意重构方案
- [ ] 优先级是否合理

