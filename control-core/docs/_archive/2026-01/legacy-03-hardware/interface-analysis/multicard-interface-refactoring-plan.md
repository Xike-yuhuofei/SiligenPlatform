# MultiCard 接口重构实施方案

## 文档信息

- 版本: 1.0.0
- 创建日期: 2025-12-09
- 状态: 规划完成，待执行
- 优先级: P0 (高风险问题，需要立即修复)

## 执行摘要

当前 `MultiCardAdapter` 实现存在严重的接口不匹配问题，错误的 `extern "C"` 声明导致函数签名与实际 MultiCard.dll 完全不符，可能引发运行时崩溃。本方案提出三步重构策略，修正接口声明并采用 C++ 类封装以提高类型安全性。

## 问题诊断

### 核心问题

1. **错误的 extern "C" 声明** (`src/infrastructure/hardware/MultiCardAdapter.cpp:19-59`)
   ```cpp
   // 当前错误声明
   short MC_Open(short cardIndex, char* localIP, short localPort, char* cardIP, short cardPort);

   // 实际 DLL 接口 (来自 docs/02-api/multicard/reference/MC_Open.md)
   int MC_Open(short nCardNum, const char* pcPCAddr, short nPCPort, const char* pmcAddr, short nmcPort);
   ```

2. **MultiCardCPP.h 是简化存根**
   - 文件注释明确说明："最小化存根版本 - 用于编译测试"
   - 缺少关键 API：MC_SetPos, MC_Update, MC_GetSts, MC_CrdSpace, MC_ArcXYC 等
   - C API 签名不完整（MC_Open 缺少网络配置参数）

3. **函数名不匹配**
   - 当前使用：`MC_GetPrfPos`
   - 实际 API：`MC_GetAxisPrfPos` (根据 MultiCardCPP.h:30)

### 影响评估

| 严重性 | 问题 | 影响 |
|--------|------|------|
| 严重 | MC_Open 参数不匹配 | 连接失败，程序无法启动 |
| 严重 | 缺少 MC_SetPos/MC_Update | 点动控制完全失效 |
| 高 | 缺少 MC_GetSts | 状态查询错误，无法监控轴状态 |
| 高 | 缺少 MC_CrdSpace | 缓冲区管理失效 |
| 中 | 缺少 MC_ArcXYC | 圆弧插补功能不可用 |

## 重构方案

### 方案概述

采用三步重构策略：
1. 完善 MultiCardCPP.h 的 C API 声明和 C++ 类
2. 重构 MultiCardAdapter 使用 C++ 类接口
3. 更新 MultiCardCPP.cpp 存根实现

### 实施步骤

#### 步骤 1: 完善 MultiCardCPP.h

**目标**: 根据 `docs/02-api/multicard/reference/` 中的文档，补全所有缺失的 API 声明

**修改清单**:

1. **修正 MC_Open 签名**
   ```cpp
   // 修改前 (行13)
   short MC_Open(short cardNum);

   // 修改后 - 与文档一致
   short MC_Open(short cardNum, const char* pcPCAddr, short nPCPort,
                 const char* pmcAddr, short nmcPort);
   ```

2. **添加缺失的 C API 声明** (在 `extern "C"` 块内)
   ```cpp
   // 点动控制 (DLL 导出函数，参考 MultiCard.def)
   short MC_PrfTrap(short axis);
   short MC_PrfJog(short axis);
   short MC_SetPos(short axis, long pos);
   short MC_SetVel(short axis, double vel);
   short MC_Update(unsigned long mask);

   // 状态查询
   short MC_GetSts(short axis, long* pSts, short count, unsigned long* pClock);
   short MC_GetPrfPos(short axis, double* pos);

   // 坐标系扩展
   short MC_CrdSpace(short crd, long* pSpace, short fifo);
   short MC_ArcXYC(short crd, long x, long y, double xCenter, double yCenter,
                   short circleDir, double synVel, double synAcc,
                   double endVel, short fifo, long segmentNum);

   // 系统控制
   short MC_Reset();

   // IO 控制
   short MC_SetExtDoBit(short doIndex, short value);
   short MC_GetDiRaw(short diIndex, short* value);
   ```

3. **扩展 C++ MultiCard 类** (第64-135行之间)
   ```cpp
   class MultiCard {
   private:
       short cardNum;

   public:
       MultiCard() : cardNum(1) {}

       // 修正 MC_Open 方法签名
       short MC_Open(const char* pcPCAddr, short nPCPort,
                     const char* pmcAddr, short nmcPort) {
           return ::MC_Open(cardNum, pcPCAddr, nPCPort, pmcAddr, nmcPort);
       }

       short MC_Close() { return ::MC_Close(cardNum); }
       short MC_Reset() { return ::MC_Reset(); }

       // 添加点动控制方法
       short MC_PrfTrap(short axis) { return ::MC_PrfTrap(axis); }
       short MC_PrfJog(short axis) { return ::MC_PrfJog(axis); }
       short MC_SetPos(short axis, long pos) { return ::MC_SetPos(axis, pos); }
       short MC_SetVel(short axis, double vel) { return ::MC_SetVel(axis, vel); }
       short MC_Update(unsigned long mask) { return ::MC_Update(mask); }

       // 添加状态查询方法
       short MC_GetSts(short axis, long* pSts, short count, unsigned long* pClock) {
           return ::MC_GetSts(axis, pSts, count, pClock);
       }
       short MC_GetPrfPos(short axis, double* pos) {
           return ::MC_GetPrfPos(axis, pos);
       }

       // 添加坐标系扩展方法
       short MC_CrdSpace(short crd, long* pSpace, short fifo) {
           return ::MC_CrdSpace(crd, pSpace, fifo);
       }
       short MC_ArcXYC(short crd, long x, long y, double xCenter, double yCenter,
                       short circleDir, double synVel, double synAcc,
                       double endVel, short fifo, long segmentNum) {
           return ::MC_ArcXYC(crd, x, y, xCenter, yCenter, circleDir,
                              synVel, synAcc, endVel, fifo, segmentNum);
       }

       // 添加 IO 控制方法
       short MC_SetExtDoBit(short doIndex, short value) {
           return ::MC_SetExtDoBit(doIndex, value);
       }
       short MC_GetDiRaw(short diIndex, short* value) {
           return ::MC_GetDiRaw(diIndex, value);
       }

       // 保持现有方法...
   };
   ```

**文件位置**: `infrastructure/hardware/MultiCardCPP.h`

#### 步骤 2: 重构 MultiCardAdapter

**2.1 修改头文件** (`src/infrastructure/hardware/MultiCardAdapter.h`)

```cpp
// 添加包含
#include "../../../infrastructure/hardware/MultiCardCPP.h"

// 修改私有成员 (第159-164行)
private:
    // 删除
    // short card_handle_;
    // int32_t card_id_;

    // 新增
    std::unique_ptr<MultiCard> multicard_;

    // 保持其他成员不变
    bool is_connected_;
    Siligen::Shared::Types::ConnectionConfig connection_config_;
    float default_speed_;
    mutable std::mutex hardware_mutex_;
```

**2.2 修改实现文件** (`src/infrastructure/hardware/MultiCardAdapter.cpp`)

**A. 删除错误的 extern "C" 声明** (第19-59行)
```cpp
// 删除整个 extern "C" 块
// extern "C" {
//     short MC_Open(...);
//     ...
// }

// 替换为
#include "../../../infrastructure/hardware/MultiCardCPP.h"
```

**B. 修改构造函数** (第72-78行)
```cpp
MultiCardAdapter::MultiCardAdapter()
    : multicard_(nullptr)  // 替代 card_handle_(-1)
    , is_connected_(false)
    , default_speed_(DEFAULT_SPEED)
{
}
```

**C. 重构 Connect() 方法** (第90-136行)
```cpp
Result<void> MultiCardAdapter::Connect(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (is_connected_) {
        return Result<void>::Failure(
            Error(ErrorCode::CONNECTION_FAILED,
                  "Already connected to hardware",
                  "MultiCardAdapter"));
    }

    // 创建 MultiCard 对象
    multicard_ = std::make_unique<MultiCard>();

    // 调用 C++ 接口（使用网络配置参数）
    char local_ip[32], card_ip[32];
    std::strncpy(local_ip, config.local_ip.c_str(), sizeof(local_ip) - 1);
    std::strncpy(card_ip, config.card_ip.c_str(), sizeof(card_ip) - 1);
    local_ip[sizeof(local_ip) - 1] = '\0';
    card_ip[sizeof(card_ip) - 1] = '\0';

    short result = multicard_->MC_Open(
        local_ip,
        static_cast<short>(config.port),
        card_ip,
        static_cast<short>(config.port)
    );

    if (result != 0) {
        multicard_.reset();
        return Result<void>::Failure(
            MapMultiCardError(result, "MC_Open"));
    }

    // 连接成功后立即复位控制卡
    short reset_result = multicard_->MC_Reset();
    if (reset_result != 0) {
        multicard_->MC_Close();
        multicard_.reset();
        return Result<void>::Failure(
            MapMultiCardError(reset_result, "MC_Reset"));
    }

    is_connected_ = true;
    connection_config_ = config;

    return Result<void>::Success();
}
```

**D. 重构 Disconnect() 方法** (第138-159行)
```cpp
Result<void> MultiCardAdapter::Disconnect() {
    std::lock_guard<std::mutex> lock(hardware_mutex_);

    if (!is_connected_) {
        return Result<void>::Success();
    }

    short result = multicard_->MC_Close();

    multicard_.reset();
    is_connected_ = false;

    return result != 0
        ? Result<void>::Failure(MapMultiCardError(result, "MC_Close"))
        : Result<void>::Success();
}
```

**E. 批量替换轴控制方法** (第170-398行)

将所有 `MC_XXX(card_handle_, ...)` 改为 `multicard_->MC_XXX(...)`：

```cpp
// EnableAxis (第185行)
short result = multicard_->MC_AxisOn(static_cast<short>(axis_id));

// DisableAxis (第210行)
short result = multicard_->MC_AxisOff(static_cast<short>(axis_id));

// HomeAxis (第235行)
short result = multicard_->MC_HomeStart(static_cast<short>(axis_id));

// MoveToPosition (第267-302行) - 点动控制
short result = multicard_->MC_PrfTrap(0);  // X轴
result = multicard_->MC_SetPos(0, x_pulse);
result = multicard_->MC_SetVel(0, actual_speed * PULSE_PER_MM);

result = multicard_->MC_PrfTrap(1);  // Y轴
result = multicard_->MC_SetPos(1, y_pulse);
result = multicard_->MC_SetVel(1, actual_speed * PULSE_PER_MM);

result = multicard_->MC_Update(0x03);  // 启动 XY 轴

// MoveAxisToPosition (第330-348行)
result = multicard_->MC_PrfTrap(axis);
result = multicard_->MC_SetPos(axis, pulse);
result = multicard_->MC_SetVel(axis, actual_speed * PULSE_PER_MM);
result = multicard_->MC_Update(1 << axis_id);

// StopAxis (第369行)
short result = multicard_->MC_Stop(static_cast<short>(axis_id), 1);

// EmergencyStop (第390行) - 需要确认签名
// 文档显示 MC_EmergencyStop() 无参数，但实际可能需要确认
short result = multicard_->MC_EmergencyStop();
```

**F. 修复轨迹执行方法** (第404-591行)

```cpp
// GetCoordinateBufferSpace (第415行)
short result = multicard_->MC_CrdSpace(static_cast<short>(crd_num), &space, 0);

// ClearCoordinateBuffer (第435行)
short result = multicard_->MC_CrdClear(static_cast<short>(crd_num));

// ExecuteLinearInterpolation (第459行)
short result = multicard_->MC_LnXY(
    static_cast<short>(crd_num),
    static_cast<long>(x_pulse),
    static_cast<long>(y_pulse),
    velocity,
    acceleration,
    end_velocity,
    fifo,
    static_cast<long>(segment_id)
);

// ExecuteArcInterpolation (第494行) - 需要修正调用
short result = multicard_->MC_ArcXYC(
    static_cast<short>(crd_num),
    static_cast<long>(x_pulse),
    static_cast<long>(y_pulse),
    cx,
    cy,
    direction,
    velocity,
    acceleration,
    end_velocity,
    fifo,
    static_cast<long>(segment_id)
);

// StartCoordinateMotion (第527行)
short result = multicard_->MC_CrdStart(static_cast<short>(crd_num));

// StopMotion (第549行) - 需要确认签名
unsigned long smooth_stop = (stop_mode == -1) ? 1 : 0;
short result = multicard_->MC_Stop(0xFF, smooth_stop);  // 停止所有轴

// TriggerCompareOutput (第574行)
short result = multicard_->MC_CmpPluse(
    static_cast<short>(crd_num),
    static_cast<long>(output_mode),
    static_cast<long>(output_pulse_count),
    static_cast<long>(pulse_width_us),
    static_cast<long>(level_before),
    static_cast<long>(level_during),
    static_cast<long>(level_after)
);
```

**G. 修正状态查询方法** (第597-688行)

关键修改：使用 `MC_GetPrfPos` 而非 `MC_GetAxisPrfPos`

```cpp
// GetAxisStatus (第621行)
double pos = 0.0;
short result = multicard_->MC_GetPrfPos(static_cast<short>(axis_id), &pos);

// GetAllAxisStatus (第648行)
if (multicard_->MC_GetPrfPos(static_cast<short>(axis), &pos) == 0) {
    status.current_position = PulseToMm(static_cast<int32_t>(pos));
}

// GetCurrentPosition (第671、677行)
result = multicard_->MC_GetPrfPos(0, &x_pos);
result = multicard_->MC_GetPrfPos(1, &y_pos);
```

**H. 修正辅助方法** (第767-778行)

```cpp
Result<uint32_t> MultiCardAdapter::GetMultiCardAxisStatus(int32_t axis_id) const {
    long status = 0;
    unsigned long clock = 0;

    short result = multicard_->MC_GetSts(
        static_cast<short>(axis_id),
        &status, 1, &clock);

    if (result != 0) {
        return Result<uint32_t>::Failure(
            MapMultiCardError(result, "MC_GetSts"));
    }

    return Result<uint32_t>::Success(static_cast<uint32_t>(status));
}
```

#### 步骤 3: 更新 MultiCardCPP.cpp

**目标**: 为新增的 C API 函数提供存根实现

**添加内容** (在 `extern "C"` 块内):

```cpp
// 点动控制实现
short MC_PrfTrap(short axis) {
    return 0;
}

short MC_PrfJog(short axis) {
    return 0;
}

short MC_SetPos(short axis, long pos) {
    return 0;
}

short MC_SetVel(short axis, double vel) {
    return 0;
}

short MC_Update(unsigned long mask) {
    return 0;
}

// 状态查询实现
short MC_GetSts(short axis, long* pSts, short count, unsigned long* pClock) {
    if (pSts) *pSts = 0;
    if (pClock) *pClock = 0;
    return 0;
}

short MC_GetPrfPos(short axis, double* pos) {
    if (pos) *pos = 0.0;
    return 0;
}

// 坐标系扩展实现
short MC_CrdSpace(short crd, long* pSpace, short fifo) {
    if (pSpace) *pSpace = 100;  // 返回100个空闲位置
    return 0;
}

short MC_ArcXYC(short crd, long x, long y, double xCenter, double yCenter,
                short circleDir, double synVel, double synAcc,
                double endVel, short fifo, long segmentNum) {
    return 0;
}

// 系统控制实现
short MC_Reset() {
    return 0;
}

// IO 控制实现
short MC_SetExtDoBit(short doIndex, short value) {
    return 0;
}

short MC_GetDiRaw(short diIndex, short* value) {
    if (value) *value = 0;
    return 0;
}
```

**文件位置**: `infrastructure/hardware/MultiCardCPP.cpp`

## API 对比表

### 核心连接 API

| 函数 | 当前签名 | 正确签名 | 优先级 |
|------|----------|----------|--------|
| MC_Open | MC_Open(cardNum) | MC_Open(cardNum, pcIP, pcPort, cardIP, cardPort) | P0 |
| MC_Close | MC_Close(cardNum) | MC_Close() [无参数] | P0 |
| MC_Reset | 不存在 | MC_Reset() [无参数] | P0 |

### 轴控制 API

| 函数 | 当前签名 | 正确签名 | 优先级 |
|------|----------|----------|--------|
| MC_AxisOn | MC_AxisOn(cardNum, axis) | MC_AxisOn(axis) | P0 |
| MC_AxisOff | MC_AxisOff(cardNum, axis) | MC_AxisOff(axis) | P0 |
| MC_PrfTrap | 不存在 | MC_PrfTrap(axis) | P0 |
| MC_SetPos | 不存在 | MC_SetPos(axis, pos) | P0 |
| MC_SetVel | 不存在 | MC_SetVel(axis, vel) | P0 |
| MC_Update | 不存在 | MC_Update(mask) | P0 |

### 状态查询 API

| 函数 | 当前签名 | 正确签名 | 优先级 |
|------|----------|----------|--------|
| MC_GetPrfPos | 不存在 | MC_GetPrfPos(axis, *pos) | P0 |
| MC_GetSts | 不存在 | MC_GetSts(axis, *sts, count, *clock) | P1 |
| MC_GetAxisPrfPos | MC_GetAxisPrfPos(cardNum, axis, *pos) | 实际使用 MC_GetPrfPos | P0 |

### 坐标系 API

| 函数 | 当前签名 | 正确签名 | 优先级 |
|------|----------|----------|--------|
| MC_LnXY | 有 | MC_LnXY(crd, x, y, vel, acc, endVel, fifo, segNum) | P0 |
| MC_CrdSpace | 不存在 | MC_CrdSpace(crd, *space, fifo) | P1 |
| MC_CrdClear | 有 | MC_CrdClear(crd) | P0 |
| MC_ArcXYC | 不存在 | MC_ArcXYC(crd, x, y, cx, cy, dir, vel, acc, endVel, fifo, segNum) | P1 |
| MC_CrdStart | 有 | MC_CrdStart(crd) | P0 |

## 风险评估与缓解

### 高风险点

1. **MC_Open 签名变更**
   - 风险：需要传递网络配置，现有调用方式可能不兼容
   - 缓解：ConnectionConfig 已包含所需的 IP 和端口信息

2. **C++ 类方法签名差异**
   - 风险：文档显示 `MC_AxisOn(axis)` 无 cardNum 参数，但当前假设需要
   - 缓解：查阅实际示例代码确认（已确认为无 cardNum）

3. **MultiCard.dll 版本兼容性**
   - 风险：实际 DLL 可能与文档不完全一致
   - 缓解：保留详细日志，运行时验证返回值

### 测试策略

1. **编译验证**
   ```bash
   cmake -B build -S . && cmake --build build --config Release
   ```

2. **链接验证**
   - 确认所有新增的 C API 能正确链接到 MultiCard.dll 或存根实现

3. **功能测试** (如果有硬件)
   - 连接测试：验证 MC_Open 能成功建立连接
   - 轴控制测试：验证使能、点动、停止功能
   - 状态查询测试：验证位置和状态读取
   - 插补测试：验证直线和圆弧插补

4. **存根模式测试** (无硬件)
   - 验证所有 API 调用返回成功（0）
   - 验证程序不崩溃

## 实施时间线

| 阶段 | 任务 | 预计工作量 | 依赖 |
|------|------|-----------|------|
| 1 | 完善 MultiCardCPP.h | 1小时 | 无 |
| 2 | 更新 MultiCardCPP.cpp 存根 | 30分钟 | 阶段1 |
| 3 | 重构 MultiCardAdapter.h | 30分钟 | 阶段1 |
| 4 | 重构 MultiCardAdapter.cpp | 2小时 | 阶段1-3 |
| 5 | 编译和链接验证 | 30分钟 | 阶段1-4 |
| 6 | 代码审查 | 1小时 | 阶段5 |
| 7 | 功能测试（可选） | 1-2小时 | 硬件可用 |

总计：约 6-8 小时

## 验收标准

- [ ] MultiCardCPP.h 包含所有必需的 C API 声明
- [ ] MultiCard C++ 类所有方法签名与文档一致
- [ ] MultiCardAdapter.cpp 删除所有错误的 extern "C" 声明
- [ ] MultiCardAdapter 使用 `std::unique_ptr<MultiCard>` 管理对象
- [ ] 所有 API 调用格式为 `multicard_->MC_XXX(...)`
- [ ] 编译无警告无错误
- [ ] 链接成功（DLL 或存根）
- [ ] 代码审查通过
- [ ] 单元测试通过（如有）

## 参考文档

- `docs/03-hardware/interface-analysis/multicard-interface-mismatch-analysis.md` - 问题分析
- `docs/02-api/multicard/reference/MC_Open.md` - MC_Open API 文档
- `docs/02-api/multicard/reference/MC_LnXY.md` - 直线插补 API
- `docs/02-api/multicard/reference/MC_AxisOn.md` - 轴使能 API
- `docs/02-api/multicard/connection.md` - 连接配置说明
- `infrastructure/hardware/MultiCardCPP.h` - 当前 C++ 接口
- `infrastructure/hardware/MultiCard.def` - DLL 导出函数列表

## 后续优化建议

1. **添加 API 版本检测**
   - 运行时检测 MultiCard.dll 版本
   - 根据版本选择不同的调用方式

2. **完善错误码映射**
   - 建立完整的 MultiCard 错误码到 ErrorCode 的映射表
   - 提供详细的错误描述

3. **性能优化**
   - 状态查询使用读写锁替代互斥锁
   - 批量操作减少锁开销

4. **单元测试扩展**
   - 为每个 API 方法添加单元测试
   - 使用 Mock 对象隔离硬件依赖

---

**最后更新**: 2025-12-09
**负责人**: Claude Code
**审核状态**: 待审核
