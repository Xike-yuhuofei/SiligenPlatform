# MultiCard接口分析报告

## 问题概述

当前项目中的 `MultiCardAdapter` 实现与实际的 `MultiCard.dll` 接口存在不匹配问题，可能导致运行时错误。

## 分析发现

### 1. MultiCard.dll 实际接口

根据 `infrastructure/hardware/MultiCardCPP.h`，MultiCard 提供的是 C++ 类接口：

```cpp
class MultiCard {
private:
    short cardNum;

public:
    MultiCard() : cardNum(1) {}
    short MC_Open() { return ::MC_Open(cardNum); }
    short MC_AxisOn(short axis) { return ::MC_AxisOn(cardNum, axis); }
    short MC_AxisOff(short axis) { return ::MC_AxisOff(cardNum, axis); }
    short MC_LnXY(short crd, long x_pulse, long y_pulse,
                  double synVel, double synAcc, double endVel,
                  short fifo, long segmentNum);
    // ...
};
```

### 2. 当前实现的接口

在 `src/infrastructure/hardware/MultiCardAdapter.cpp` 中，错误地使用了 `extern "C"` 声明：

```cpp
extern "C" {
    // 错误的函数签名
    short MC_Open(short cardIndex, char* localIP, short localPort, char* cardIP, short cardPort);
    // ...
}
```

### 3. 函数签名不匹配

| 函数名 | 当前实现 | 实际接口 | 状态 |
|--------|----------|----------|------|
| MC_Open | MC_Open(cardIndex, localIP, localPort, cardIP, cardPort) | MC_Open(cardNum) | ❌ 不匹配 |
| MC_AxisOn | MC_AxisOn(cardHandle, axis) | MC_AxisOn(cardNum, axis) | ⚠️ 参数名不同 |
| MC_Close | MC_Close(cardHandle) | MC_Close(cardNum) | ⚠️ 参数名不同 |
| MC_GetPrfPos | MC_GetPrfPos(cardHandle, axis, &pos) | MC_GetAxisPrfPos(cardNum, axis, &pos) | ❌ 函数名不同 |

## 影响评估

### 高风险问题
1. **连接失败**: `MC_Open` 函数签名完全不匹配，可能导致连接失败
2. **状态查询错误**: `MC_GetPrfPos` 函数名不匹配，可能导致运行时错误
3. **内存访问错误**: 错误的参数类型可能导致内存访问异常

### 中等风险问题
1. **运动控制不稳定**: 参数名差异可能导致运动控制异常
2. **错误处理失效**: 错误码映射可能不准确

## 修正方案

### 方案1: 使用 C++ 类接口（推荐）

```cpp
// MultiCardAdapter.h
#include "MultiCardCPP.h"

class MultiCardAdapter : public IHardwareController {
private:
    std::unique_ptr<MultiCard> multicard_;

public:
    Result<void> Connect(const ConnectionConfig& config) override;
    Result<void> EnableAxis(int32_t axis_id) override;
    // ...
};
```

```cpp
// MultiCardAdapter.cpp
Result<void> MultiCardAdapter::Connect(const ConnectionConfig& config) {
    multicard_ = std::make_unique<MultiCard>();
    short result = multicard_->MC_Open();

    if (result != 0) {
        return Result<void>::Failure(
            MapMultiCardError(result, "MC_Open"));
    }

    is_connected_ = true;
    return Result<void>::Success();
}
```

### 方案2: 修正 C 接口声明

```cpp
// 修正后的 extern "C" 声明
extern "C" {
    short MC_Open(short cardNum);
    short MC_Close(short cardNum);
    short MC_AxisOn(short cardNum, short axis);
    short MC_AxisOff(short cardNum, short axis);
    short MC_GetAxisPrfPos(short cardNum, short axis, double* pos);
    // ...
}
```

## 推荐行动

1. **立即行动**: 使用方案1重构 `MultiCardAdapter`
2. **测试验证**: 重构后进行完整的连接和运动控制测试
3. **文档更新**: 更新相关技术文档和接口说明

## 风险缓解

在修复前，可以采取以下临时措施：

1. **添加错误日志**: 详细记录 MultiCard 调用的错误信息
2. **增强异常处理**: 捕获可能的访问违规异常
3. **回滚机制**: 准备回滚到稳定版本的机制

## 结论

当前实现与 MultiCard.dll 的实际接口存在严重不匹配问题，需要立即修正以确保系统的稳定性和可靠性。建议采用 C++ 类接口方案进行重构。

---
*分析日期: 2025-12-09*
*版本: 1.0.0*