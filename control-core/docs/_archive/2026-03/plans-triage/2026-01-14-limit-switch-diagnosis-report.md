# 限位开关检测问题诊断报告

**日期**: 2026-01-14
**问题**: X轴限位开关一直处于触发状态，但软件无法检测到
**状态**: 🔍 诊断中 - 发现根本原因

---

## 执行摘要

用户反馈X轴（轴1）的限位开关一直处于触发状态，但通过HTTP API查询显示限位状态为`false`（未触发）。经过深入诊断，发现了硬件接线和软件读取方式之间的**根本性不匹配**。

---

## 关键发现

### 1. 硬件接线方式（用户确认）

```
X轴限位开关 → 接在运动控制卡的 "Homing1和2" 输入上
```

**关键信息**: 限位开关连接到**Home信号输入**，而不是标准的限位信号输入。

### 2. 软件读取方式（代码分析）

系统存在**两个不同的类**都实现了`GetAxisStatus`:

#### 2.1 MultiCardAdapter (src/infrastructure/hardware/MultiCardAdapter.cpp:930-948)

```cpp
AxisStatusStruct MultiCardAdapter::ConvertAxisStatus(int32_t axis_id, uint32_t mc_status) const {
    // 解析MultiCard状态位
    // bit 3: 正限位
    // bit 4: 负限位
    // bit 8: 伺服使能
    // bit 10: 到位信号

    bool pos_limit = (mc_status & (1 << 3)) != 0;
    bool neg_limit = (mc_status & (1 << 4)) != 0;

    status.hard_limit_positive = pos_limit;
    status.hard_limit_negative = neg_limit;
}
```

**读取方式**: 从`MC_GetSts`返回的SDK状态位读取
**限位位**: bit 3 = 正限位, bit 4 = 负限位

#### 2.2 MultiCardMotionAdapter (src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp:280-329)

```cpp
Result<MotionStatus> MultiCardMotionAdapter::GetAxisStatus(short axis) const {
    // 检查硬件限位状态
    auto pos_limit_result = IsHardLimitTriggered(axis, true);
    if (pos_limit_result.IsSuccess()) {
        status.hard_limit_positive = pos_limit_result.Value();
    }

    auto neg_limit_result = IsHardLimitTriggered(axis, false);
    if (neg_limit_result.IsSuccess()) {
        status.hard_limit_negative = neg_limit_result.Value();
    }
}

Result<bool> MultiCardMotionAdapter::IsHardLimitTriggered(short axis, bool positive) const {
    // 读取Home信号状态（因为X/Y轴限位开关接在Home输入上）
    // nDiType: 3=原点开关
    long limit_state = 0;
    short ret = hardware_wrapper_->MC_GetDiRaw(3, &limit_state);

    // Home信号位对应：轴N对应bit(N-1)
    bool triggered = (limit_state & (1 << axis)) != 0;
}
```

**读取方式**: 调用`MC_GetDiRaw(3, &limit_state)`读取Home信号
**限位位**: 通过DI信号读取，nDiType=3表示Home信号

### 3. API签名修复

#### 修复前（错误）:
```cpp
// IMultiCardWrapper.h:407
virtual int MC_GetDiRaw(short nCardIndex, long* pValue) noexcept = 0;
```

**问题**: 第一个参数被误认为是`nCardIndex`，实际应该是`nDiType`

#### 修复后（正确）:
```cpp
// IMultiCardWrapper.h:407
virtual int MC_GetDiRaw(short nDiType, long* pValue) noexcept = 0;
// nDiType: 0=正限位, 1=负限位, 2=驱动报警, 3=原点开关, 4=通用输入, 7=手轮IO
```

### 4. 调试输出异常

**添加的调试代码**:
- `MultiCardMotionAdapter::GetAxisStatus`开头
- `MultiCardMotionAdapter::IsHardLimitTriggered`开头和中间
- `MultiCardAdapter::ConvertAxisStatus`开头

**观察结果**:
- ❌ `GetAxisStatus`的调试输出**完全没有出现**
- ❌ `IsHardLimitTriggered`的调试输出**完全没有出现**
- ✅ `SetHardLimits`的调试输出正常出现

**推测**:
1. 系统可能正在使用缓存的旧版本二进制文件
2. 或者有其他机制在拦截/代理状态查询
3. 或者使用了不同的实现类

---

## 诊断步骤记录

### 步骤1: 确认API返回值
```bash
curl http://localhost:8080/api/motion/status?axis=0
# 返回: "hardLimit": {"positive": false, "negative": false}
```

### 步骤2: 添加调试输出
在多个函数中添加`std::cout`调试输出，但未看到任何输出。

### 步骤3: 发现代码架构问题
发现两个类都实现了`IMotionStatePort`:
- `MultiCardAdapter` - 从SDK状态位读取限位
- `MultiCardMotionAdapter` - 调用`IsHardLimitTriggered`读取限位

### 步骤4: 检查注册代码
```cpp
// ApplicationContainer.cpp:290-296
auto motionAdapter = std::make_shared<Infrastructure::Adapters::MultiCardMotionAdapter>(
    multiCardWrapper, hwConfig.Value()
);
motion_port_ = motionAdapter;
RegisterPort<Domain::Ports::IMotionControllerPort>(motionAdapter);
RegisterPort<Domain::Ports::IMotionStatePort>(motionAdapter);
```

**确认**: 注册的是`MultiCardMotionAdapter`

### 步骤5: 检查API处理代码
```cpp
// MotionHandler.cpp:514-521
auto motion_port = container_->ResolvePort<Domain::Ports::IMotionStatePort>();
auto result = motion_port->GetAxisStatus(axis_id);
```

**确认**: 通过`ResolvePort`获取端口

---

## 2026-01-14 19:53 深度诊断结果

### 🔴 **关键发现：接口继承架构问题**

#### 问题1：GetAxisStatus 调用未到达实现类

**测试结果**：
1. ✅ HTTP层调试输出正常：`[HTTP DEBUG] About to call GetAxisStatus for axis 0`
2. ❌ MultiCardMotionAdapter::GetAxisStatus 调试输出**完全没有出现**
3. ❌ MultiCardAdapter::GetAxisStatus 调试输出**完全没有出现**
4. ✅ 系统注册类型确认：`typeid: class Siligen::Infrastructure::Adapters::MultiCardMotionAdapter`

**根本原因分析**：

```
继承链分析：

IMotionStatePort (定义GetAxisStatus)
    ↑
    └─ [缺失!] MultiCardMotionAdapter 未直接继承 IMotionStatePort
         ↑
    MotionControllerPortBase (继承 IMotionControllerPort，但不含 GetAxisStatus)
         ↑
         └─ MultiCardMotionAdapter (实际只继承这个)
```

**关键代码证据**：

```cpp
// src/infrastructure/adapters/hardware/MultiCardMotionAdapter.h:26
class MultiCardMotionAdapter : public Domain::Ports::MotionControllerPortBase {
    // 声明了 GetAxisStatus override，但基类链中没有这个接口！
    Result<Domain::Ports::MotionStatus> GetAxisStatus(short axis) const override;
};

// src/domain/motion/ports/MotionControllerPortBase.h:11
class MotionControllerPortBase : public IMotionControllerPort {
    // 不包含 GetAxisStatus，因为 GetAxisStatus 在 IMotionStatePort 中定义
};

// src/domain/motion/ports/IMotionStatePort.h:80
class IMotionStatePort {
    virtual Result<MotionStatus> GetAxisStatus(short axis) const = 0;
};
```

**问题**：`MultiCardMotionAdapter` 使用 `override` 声明 `GetAxisStatus`，但继承链中不包含定义该函数的 `IMotionStatePort` 接口！

#### 问题2：API签名已修复

```cpp
// IMultiCardWrapper.h:407 - 已修复 ✓
virtual int MC_GetDiRaw(short nDiType, long* pValue) noexcept = 0;
// nDiType: 0=正限位, 1=负限位, 2=驱动报警, 3=原点开关, 4=通用输入, 7=手轮IO
```

#### 问题3：代理问题

```bash
# curl 配置问题（已解决）
all_proxy='socks5://127.0.0.1:7890'  # 导致请求被代理拦截

# 解决方案
curl --noproxy localhost http://localhost:8080/api/motion/status?axis=0
```

---

## 可能的根本原因

### 原因A: SDK状态位可能不反映Home信号状态

MultiCard SDK的`MC_GetSts`返回的状态位可能**不包含**Home信号状态：
- bit 3: 正限位（来自限位信号输入，不是Home输入）
- bit 4: 负限位（来自限位信号输入，不是Home输入）

如果硬件将限位开关接到Home输入，那么SDK状态位的bit 3/4将永远是0（未触发），导致无法检测到限位开关状态。

### 原因B: 需要明确读取Home信号

根据MultiCard文档，`MC_GetDiRaw`可以读取不同类型的DI信号：
- `nDiType=0`: 正限位信号
- `nDiType=1`: 负限位信号
- `nDiType=3`: **Home信号** ← 这是我们的硬件接线方式！

### 原因C: 调试输出未出现的可能原因

1. **编译缓存**: 可能使用了旧的二进制文件
2. **输出缓冲**: `std::cout`可能被缓冲，使用`--no-log-file`参数应该禁用了日志文件但可能有其他缓冲机制
3. **代码路径错误**: 可能调用了不同的实现类

---

## 解决方案建议

### 方案1: 修改MultiCardAdapter以读取Home信号（推荐）

**目标文件**: `src/infrastructure/hardware/MultiCardAdapter.cpp:791-829`

**修改思路**:
```cpp
Result<AxisStatusStruct> MultiCardAdapter::GetAxisStatus(int32_t axis_id) const {
    // ... 现有代码 ...

    // 不从SDK状态位读取限位，而是直接读取Home信号
    long home_signal_state = 0;
    short ret = multicard_->MC_GetDiRaw(3, &home_signal_state);  // nDiType=3表示Home信号

    if (ret == 0) {
        // Home信号位映射：轴N对应bit(N-1)
        bool pos_limit = (home_signal_state & (1 << axis_id)) != 0;
        bool neg_limit = false;  // 暂时假设只有一个Home开关

        status.hard_limit_positive = pos_limit;
        status.hard_limit_negative = neg_limit;
    } else {
        // 错误处理
    }
}
```

**优点**:
- 统一使用`MultiCardAdapter`读取Home信号
- 代码修改集中在一个文件

**缺点**:
- 需要理解`MultiCardAdapter`和`MultiCardMotionAdapter`的职责划分

### 方案2: 确保使用MultiCardMotionAdapter

**检查**:
1. 确认编译链接了正确的实现
2. 确认`ResolvePort<IMotionStatePort>()`返回的是`MultiCardMotionAdapter`
3. 添加类型检查或日志确认使用的类

**实现**:
```cpp
// ApplicationContainer.cpp
auto motionAdapter = std::make_shared<Infrastructure::Adapters::MultiCardMotionAdapter>(...);
std::cout << "[ApplicationContainer] Registered type: "
          << typeid(*motionAdapter).name() << std::endl;
```

### 方案3: 直接从MC_GetSts读取Home信号位（如果支持）

查阅MultiCard文档，确认`MC_GetSts`返回的状态位是否包含Home信号信息。如果包含，找到正确的bit位并修改`ConvertAxisStatus`。

---

## 下一步行动

### 立即行动（高优先级）
1. ✅ **已修复**: `IMultiCardWrapper.h`中的`MC_GetDiRaw`签名
2. ✅ **已修复**: `MultiCardMotionAdapter::IsHardLimitTriggered`以读取Home信号
3. ❓ **待确认**: 为什么调试输出没有出现？
   - 检查是否有多个`IMotionStatePort`注册
   - 强制重新编译（删除CMakeCache.txt）
   - 检查是否有代理/拦截器模式

### 后续行动（中优先级）
1. **确定使用哪个实现类**: 在`ApplicationContainer`中添加类型日志
2. **选择修复方案**: 方案1或方案2
3. **测试验证**: 确认能正确检测X轴限位开关状态

### 长期优化（低优先级）
1. **统一架构**: 明确`MultiCardAdapter`和`MultiCardMotionAdapter`的职责划分
2. **添加集成测试**: 自动化测试限位开关检测
3. **文档完善**: 记录硬件接线和软件读取方式的对应关系

---

## 技术细节

### MultiCard API参考

#### MC_GetDiRaw
```cpp
int MC_GetDiRaw(short nDiType, long *pValue);

// nDiType参数:
//   0: 正限位信号 (positive limit)
//   1: 负限位信号 (negative limit)
//   2: 驱动报警 (drive alarm)
//   3: 原点开关 (home switch) ← 我们的硬件接线方式
//   4: 通用输入 (general input)
//   7: 手轮IO (handwheel IO)

// pValue返回值:
//   bit N: 轴N+1的DI信号状态
```

#### MC_GetSts
```cpp
int MC_GetSts(short nAxis, long *pState);

// pState返回值的关键位:
//   bit 3: 正限位 (来自限位信号输入，不是Home输入)
//   bit 4: 负限位 (来自限位信号输入，不是Home输入)
//   bit 8: 伺服使能
//   bit 10: 到位信号
```

### 硬件接线对照表

| 轴 | 限位开关接线 | 软件读取方式 | 状态 |
|----|-------------|-------------|------|
| X轴 | Homing1/2输入 | 应该读取: MC_GetDiRaw(3) | 未确认 |
| Y轴 | Homing3/4输入 (假设) | 应该读取: MC_GetDiRaw(3) | 未确认 |

---

## 附录

### A. 相关文件清单

**核心实现**:
- `src/infrastructure/hardware/IMultiCardWrapper.h:407` - API接口定义（已修复）
- `src/infrastructure/hardware/MultiCardAdapter.cpp:791-829` - GetAxisStatus实现
- `src/infrastructure/hardware/MultiCardAdapter.cpp:930-970` - ConvertAxisStatus实现
- `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp:280-329` - GetAxisStatus实现（已修改）
- `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp:880-933` - IsHardLimitTriggered实现（已修改）

**注册和使用**:
- `apps/control-runtime/container/ApplicationContainer.cpp:290-298` - 适配器注册
- `src/adapters/http/handlers/MotionHandler.cpp:490-565` - HTTP处理

**文档**:
- `third_party/vendor/01-基础功能/03-IO操作API.md` - MultiCard API文档

### B. 调试命令

```bash
# 1. 查询轴状态
curl http://localhost:8080/api/motion/status?axis=0

# 2. 查询所有轴状态
curl http://localhost:8080/api/motion/status/all

# 3. 连接硬件
curl -X POST http://localhost:8080/api/hardware/connect \
  -H "Content-Type: application/json" \
  -d '{"cardIp":"192.168.0.1","localIp":"192.168.0.200"}'

# 4. 启动服务器（无日志文件）
cd build/bin && ./HardwareHttpServer.exe --no-log-file 2>&1 &

# 5. 监控DI状态（PowerShell脚本）
.\scripts\monitor_di.ps1
```

### C. 编译命令

```bash
# 强制重新编译
cd build
cmake --build . --target HardwareHttpServer --clean-first

# 或使用优化脚本
.\scripts\optimize-build.ps1 -Fast
```

---

**报告生成时间**: 2026-01-14 19:31
**Token使用**: 108743/200000
**下次继续**: 基于本诊断报告直接实施修复方案


