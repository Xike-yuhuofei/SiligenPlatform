# 实施计划系统化审计清单

**版本:** 1.0.0
**适用范围:** siligen-motion-controller 项目所有实施计划
**最后更新:** 2026-01-09

---

## 一、审计清单使用指南

### 目的
本清单提供标准化的审计流程，确保所有实施计划在开始执行前通过架构合规性、安全性、性能和可测试性检查。

### 使用方法
1. **计划作者自检:** 在提交计划前使用清单自我检查
2. **架构师审查:** 使用清单进行正式审计
3. **AI辅助审计:** 提示词: "使用 docs/PLAN_AUDIT_CHECKLIST.md 审计当前计划"

### 评分标准
- ✅ **PASS:** 完全合规
- ⚠️ **WARNING:** 有风险但可接受（需文档化理由）
- ❌ **FAIL:** 不合规，必须修正

### 审计输出
- 所有 ❌ FAIL 项必须修正才能实施
- ⚠️ WARNING 项必须在计划中添加风险说明
- 生成 `docs/audits/YYYY-MM-DD-<plan-name>-audit-report.md`

---

## 二、架构合规性审计

### A. 六边形架构依赖方向 (HEXAGONAL.md)

#### A1. 领域层依赖检查
**检查项:**
```
□ 领域层代码不包含 #include "../../infrastructure/**"
□ 领域层代码不包含 #include "../../adapters/**"
□ 领域层仅依赖 shared/** 和 domain/<subdomain>/ports/**
□ 领域服务通过端口接口访问外部功能
```

**验证方法:**
```bash
# 自动检查脚本
rg "#include.*infrastructure|#include.*adapters" src/domain/ --glob '*.h' --glob '*.cpp'
# 预期输出: No matches found
```

**评分规则:**
- 任何违规 → ❌ FAIL

---

#### A2. 端口接口纯虚性检查
**检查项:**
```
□ 新增/修改的端口接口均为纯虚函数 (= 0)
□ 端口接口无成员变量
□ 端口接口无实现逻辑
□ 端口接口文件位于 src/domain/<subdomain>/ports/**
```

**验证方法:**
```bash
# 检查端口文件是否包含实现
rg "^[^/]*\{" src/domain/<subdomain>/ports/ --glob '*.h' | grep -v "^namespace"
# 预期输出: 空（端口头文件不应有函数体）
```

**评分规则:**
- 端口接口包含实现 → ❌ FAIL
- 端口接口有成员变量 → ❌ FAIL

---

#### A3. 应用层依赖检查
**检查项:**
```
□ UseCase 通过构造函数注入端口依赖
□ UseCase 不直接 #include 硬件适配器
□ UseCase 不直接调用 MultiCard API
□ UseCase 返回 Result<T> 而非抛出异常
```

**验证方法:**
- 代码审查: 检查构造函数签名
- 搜索违规包含: `rg "MultiCard|lnxy|driver" src/application/`

**评分规则:**
- 直接依赖硬件 → ❌ FAIL

---

### B. 命名空间规范 (NAMESPACE.md)

#### B1. 根命名空间强制
**检查项:**
```
□ 所有新代码使用 Siligen:: 根命名空间
□ 领域层代码使用 Siligen::Domain::*
□ 应用层代码使用 Siligen::Application::*
□ 基础设施层代码使用 Siligen::Infrastructure::*
□ 共享代码使用 Siligen::Shared::*
```

**验证方法:**
```bash
# 检查新文件是否包含正确命名空间
rg "^namespace " <new-files> | grep -v "Siligen::"
# 预期输出: 空
```

**评分规则:**
- 使用全局命名空间 → ❌ FAIL
- 使用错误的层命名空间 → ❌ FAIL

---

#### B2. 头文件using指令禁令
**检查项:**
```
□ 头文件不包含 using namespace std;
□ 头文件不包含 using namespace <任何>;
□ 头文件仅使用 using 类型别名声明（如 using float32 = ...）
```

**验证方法:**
```bash
rg "^using namespace" src/ --glob '*.h'
# 预期输出: No matches found
```

**评分规则:**
- 头文件包含 using namespace → ❌ FAIL

---

### C. 端口与适配器规范 (PORTS_ADAPTERS.md)

#### C1. 端口命名约定
**检查项:**
```
□ 端口接口名称以 I 开头（如 IMotionControlPort）
□ 端口接口名称以 Port 结尾
□ 端口接口文件名与类名匹配（IMotionControlPort.h）
```

**验证方法:**
- 代码审查: 检查新增端口接口命名

**评分规则:**
- 命名不符合约定 → ⚠️ WARNING（建议重命名）

---

#### C2. 适配器单一职责
**检查项:**
```
□ 每个适配器仅实现一个端口接口
□ 适配器不包含业务逻辑
□ 适配器仅负责协议转换（Domain ↔ Infrastructure）
```

**验证方法:**
- 代码审查: 检查适配器类继承列表
- 检查适配器是否包含复杂逻辑（循环、条件判断超过3层）

**评分规则:**
- 适配器实现多个端口 → ❌ FAIL
- 适配器包含业务逻辑 → ❌ FAIL

---

## 三、领域层特殊约束审计

### D. 领域层禁令 (DOMAIN.md)

#### D1. 动态内存分配禁令
**检查项:**
```
□ 领域层代码不使用 new/delete
□ 领域层代码不使用 malloc/free
□ 领域层代码不使用 std::make_shared/std::make_unique
```

**验证方法:**
```bash
rg "(new |delete |malloc |free |make_shared|make_unique)" src/domain/ \
  --glob '*.cpp' --glob '*.h'
```

**评分规则:**
- 任何违规 → ❌ FAIL（除非有 CLAUDE_SUPPRESS 抑制）

---

#### D2. STL容器禁令
**检查项:**
```
□ 领域层代码不使用 std::vector
□ 领域层代码不使用 std::map/std::unordered_map
□ 领域层代码不使用 std::list/std::deque
□ 例外: src/domain/motion/** 允许轨迹点容器
□ 例外: 返回值可使用 std::vector（运行时大小不确定）
```

**验证方法:**
```bash
# 排除motion目录
rg "std::(vector|map|unordered_map|list|deque)" src/domain/ \
  --glob '*.cpp' --glob '*.h' \
  --glob '!motion/**'
```

**评分规则:**
- 违规且无抑制 → ❌ FAIL
- 违规有抑制但理由不充分 → ⚠️ WARNING

---

#### D3. I/O和线程禁令
**检查项:**
```
□ 领域层代码不使用 std::cout/std::cerr
□ 领域层代码不使用 std::thread
□ 领域层代码不使用 std::mutex（应在应用层处理）
□ 领域层代码不包含文件I/O操作
```

**验证方法:**
```bash
rg "std::(cout|cerr|thread|mutex|fstream|ifstream|ofstream)" src/domain/
```

**评分规则:**
- 任何违规 → ❌ FAIL

---

#### D4. 公共API noexcept强制
**检查项:**
```
□ 领域层公共方法标记为 noexcept
□ 领域层私有方法可不标记（内部实现）
□ 错误通过 Result<T> 返回，而非异常
```

**验证方法:**
- 代码审查: 检查新增公共方法签名
- 搜索 throw 关键字: `rg "throw " src/domain/`

**评分规则:**
- 公共方法缺少 noexcept → ⚠️ WARNING
- 使用 throw 异常 → ❌ FAIL

---

#### D5. 全局可变状态禁令
**检查项:**
```
□ 领域层无 static 非 const 变量
□ 领域层无全局变量
□ 领域层服务无静态成员变量（除 constexpr）
```

**验证方法:**
```bash
rg "static .*=" src/domain/ | rg -v "const|constexpr"
```

**评分规则:**
- 任何违规 → ❌ FAIL

---

## 四、实时安全审计 (INDUSTRIAL.md)

### E. 实时路径检查

#### E1. 关键路径动态分配检查
**检查项:**
```
□ 轨迹执行热路径无堆分配
□ 插补算法内部无 std::vector 创建
□ 运动控制循环无系统调用
```

**验证方法:**
- 代码审查: 标记热路径代码
- 使用 Valgrind/massif 分析堆分配

**评分规则:**
- 关键路径有堆分配 → ❌ FAIL
- 非关键路径有堆分配 → ✅ PASS

---

#### E2. 无界循环禁令
**检查项:**
```
□ 实时代码所有循环有上界
□ 循环次数可静态计算或有明确最大值
□ 无 while(true) 或 for(;;)（除非是主控制循环）
```

**验证方法:**
```bash
rg "while\s*\(true\)|for\s*\(\s*;\s*;\s*\)" src/domain/motion/
```

**评分规则:**
- 无界循环无上界保护 → ❌ FAIL

---

#### E3. 阻塞操作禁令
**检查项:**
```
□ 实时代码无 sleep/usleep
□ 实时代码无无超时的锁等待
□ 实时代码无网络I/O
```

**验证方法:**
```bash
rg "(sleep|usleep|lock\(\)|recv|send)" src/domain/motion/
```

**评分规则:**
- 阻塞操作 → ❌ FAIL

---

### F. 工业安全特殊检查

#### F1. 紧急停止路径检查
**检查项:**
```
□ EmergencyStop 实现无条件执行
□ EmergencyStop 路径无日志记录
□ EmergencyStop 路径无中间步骤
□ EmergencyStop 直接调用硬件命令
□ EmergencyStop 方法标记为 noexcept
```

**验证方法:**
- 代码审查: `src/application/usecases/EmergencyStop.cpp`
- 确认代码路径 < 10行

**评分规则:**
- 紧急停止路径有条件/日志 → ❌ FAIL

---

#### F2. 缓冲区安全检查
**检查项:**
```
□ BufferManager 写入前检查剩余空间
□ BufferManager 处理上溢/下溢
□ 环形缓冲区索引使用模运算
```

**验证方法:**
- 代码审查: `src/domain/motion/BufferManager.cpp`
- 搜索边界检查: `rg "if.*space|if.*full|if.*empty"`

**评分规则:**
- 缺少边界检查 → ❌ FAIL

---

#### F3. 阀门状态安全检查
**检查项:**
```
□ 阀门状态机跟踪所有状态转换
□ 错误时阀门默认关闭（fail-closed）
□ 阀门操作有超时保护
```

**验证方法:**
- 代码审查: 检查阀门相关 UseCase

**评分规则:**
- 缺少超时/fail-closed → ⚠️ WARNING

---

## 五、硬件隔离审计 (HARDWARE.md)

### G. 硬件域隔离

#### G1. 领域层硬件零知识
**检查项:**
```
□ 领域层代码不包含 MultiCard API 调用
□ 领域层代码不包含驱动程序头文件
□ 领域层代码不包含硬件错误码（如 0x1001）
□ 领域层仅通过 IHardwareController 端口访问硬件
```

**验证方法:**
```bash
rg "#include.*MultiCard|#include.*lnxy|#include.*driver|0x[0-9A-Fa-f]{4}" \
  src/domain/
```

**评分规则:**
- 任何违规 → ❌ FAIL（绝对禁止）

---

#### G2. 硬件适配器错误转换
**检查项:**
```
□ 硬件适配器将硬件错误码转换为 Result<T>
□ 硬件适配器不抛出异常
□ 硬件错误映射到语义化错误（AXIS_NOT_READY 而非 0x1001）
```

**验证方法:**
- 代码审查: `src/infrastructure/adapters/hardware/**`
- 确认无 throw 关键字

**评分规则:**
- 适配器抛出异常 → ❌ FAIL
- 错误码未转换 → ⚠️ WARNING

---

#### G3. 硬件适配器线程安全
**检查项:**
```
□ 硬件卡访问使用 mutex 保护
□ 卡选择操作线程安全
□ 无静态硬件句柄
```

**验证方法:**
```bash
rg "std::mutex|std::lock_guard" src/infrastructure/adapters/hardware/
```

**评分规则:**
- 共享硬件资源无锁保护 → ❌ FAIL

---

## 六、性能与数值稳定性审计

### H. 浮点运算检查

#### H1. 浮点比较epsilon容差
**检查项:**
```
□ 边界比较使用 epsilon 容差（推荐 1e-6f）
□ 不使用精确相等 (a == b) 比较浮点数
□ 提供辅助函数 FloatEqual/FloatLess/FloatGreater
```

**验证方法:**
- 代码审查: 搜索浮点比较 `rg "float.*==|double.*=="`
- 检查是否有 epsilon 定义

**评分规则:**
- 精确比较浮点边界值 → ⚠️ WARNING
- 关键安全边界精确比较 → ❌ FAIL

---

#### H2. 数值溢出保护
**检查项:**
```
□ 大数乘法前检查溢出
□ 累加循环使用 double 避免精度损失
□ 除法前检查除数非零
```

**验证方法:**
- 代码审查: 检查数学运算密集代码

**评分规则:**
- 除零未检查 → ❌ FAIL
- 溢出未检查 → ⚠️ WARNING

---

#### H3. 单位一致性检查
**检查项:**
```
□ 所有距离单位统一为 mm
□ 所有速度单位统一为 mm/s
□ 所有加速度单位统一为 mm/s²
□ 函数参数注释包含单位
```

**验证方法:**
- 代码审查: 检查函数参数注释

**评分规则:**
- 单位不一致 → ❌ FAIL

---

### I. 性能约束检查

#### I1. 配置加载性能
**检查项:**
```
□ 配置在UseCase执行开始时加载一次
□ 配置加载失败有明确错误信息
□ 重复加载配置有缓存机制
```

**验证方法:**
- 代码审查: 检查 LoadConfiguration 调用位置
- 计数调用次数（执行100个线段应只加载1次）

**评分规则:**
- 每个线段重复加载配置 → ⚠️ WARNING（性能浪费）

---

#### I2. 大数据集验证性能
**检查项:**
```
□ 轨迹验证算法为 O(n)
□ 10万点验证耗时 < 100ms
□ 提供性能基准测试
```

**验证方法:**
- 代码审查: 检查嵌套循环
- 运行性能测试: `TEST(...Performance...)`

**评分规则:**
- 算法复杂度 > O(n) → ⚠️ WARNING
- 性能测试缺失 → ⚠️ WARNING

---

## 七、线程安全审计

### J. 并发访问检查

#### J1. 共享状态保护
**检查项:**
```
□ 多线程访问的成员变量使用 mutex 保护
□ 原子操作使用 std::atomic
□ 无裸指针并发修改
```

**验证方法:**
- 代码审查: 检查有后台线程的类
- 搜索 `std::thread` 相关类

**评分规则:**
- 共享状态无保护 → ❌ FAIL

---

#### J2. 回调函数线程安全
**检查项:**
```
□ 回调函数设置/调用使用同一把锁
□ 回调函数空指针检查在锁内
□ 回调函数异常不会导致线程崩溃
```

**验证方法:**
- 代码审查: 检查回调函数调用位置
- 确认 try-catch 包裹用户回调

**评分规则:**
- 回调并发访问无锁 → ❌ FAIL
- 用户回调异常未捕获 → ⚠️ WARNING

---

#### J3. 线程退出竞态条件
**检查项:**
```
□ Stop() 方法检查是否在监控线程内调用
□ 避免监控线程 join 自己（死锁）
□ Stop() 方法幂等（多次调用安全）
```

**验证方法:**
- 代码审查: 检查 Stop() 实现
- 测试: 并发调用 Stop() 100次

**评分规则:**
- 可能死锁 → ❌ FAIL

---

## 八、测试完备性审计

### K. 单元测试覆盖

#### K1. 核心逻辑测试
**检查项:**
```
□ 每个公共方法至少有1个测试
□ 边界条件有专门测试（最小值、最大值、零）
□ 错误路径有测试（参数非法、依赖失败）
```

**验证方法:**
- 代码审查: 检查测试文件
- 测试覆盖率工具（如需要）

**评分规则:**
- 核心方法无测试 → ❌ FAIL
- 仅有快乐路径测试 → ⚠️ WARNING

---

#### K2. 反例测试
**检查项:**
```
□ 几何反例（圆弧顶点超限、椭圆长轴超限）
□ 数值反例（浮点累积误差、单位转换误差）
□ 并发反例（回调竞态、配置竞态）
□ 边界反例（边界包含语义、零尺寸工作区）
```

**验证方法:**
- 代码审查: 检查测试用例名称包含 "Edge", "Boundary", "Race", "Concurrent"

**评分规则:**
- 缺少反例测试 → ⚠️ WARNING

---

#### K3. 性能回归测试
**检查项:**
```
□ 关键路径有性能基准测试
□ 测试验证时间上界（如 "< 100ms"）
□ 测试验证算法复杂度（如 O(n)）
```

**验证方法:**
- 搜索测试名称: `rg "Performance|Benchmark"`

**评分规则:**
- 性能敏感代码无基准测试 → ⚠️ WARNING

---

### L. 集成测试覆盖

#### L1. 端到端流程测试
**检查项:**
```
□ UseCase 执行完整流程有测试
□ 测试覆盖正常执行和异常中止
□ 测试使用 Mock 端口而非真实硬件
```

**验证方法:**
- 检查集成测试文件: `tests/integration/`

**评分规则:**
- UseCase 无集成测试 → ⚠️ WARNING

---

#### L2. 依赖失败场景测试
**检查项:**
```
□ 配置加载失败测试
□ 硬件连接失败测试
□ 端口返回错误测试
```

**验证方法:**
- 搜索 Mock 返回失败: `rg "Return.*Failure|WillOnce.*Failure"`

**评分规则:**
- 缺少依赖失败测试 → ⚠️ WARNING

---

## 九、文档完备性审计

### M. 计划文档检查

#### M1. 实施前提条件
**检查项:**
```
□ 明确列出依赖的端口接口
□ 明确列出需要的类型定义
□ 明确列出配置文件要求
□ 明确列出实施顺序约束
```

**评分规则:**
- 缺少前提条件 → ⚠️ WARNING

---

#### M2. 风险与限制说明
**检查项:**
```
□ 文档说明非实时路径的延迟特性
□ 文档说明已知限制（如仅2D支持）
□ 文档说明性能目标与实际指标
```

**评分规则:**
- 缺少风险说明 → ⚠️ WARNING

---

#### M3. 验收标准
**检查项:**
```
□ 每个 Task 有明确的验收标准
□ 验收标准可测试（非主观判断）
□ 提供验收测试步骤
```

**评分规则:**
- 验收标准模糊 → ⚠️ WARNING

---

## 十、规则例外处理审计

### N. CLAUDE_SUPPRESS 抑制检查

#### N1. 抑制格式验证
**检查项:**
```
□ 抑制注释包含 RULE_NAME
□ 抑制注释包含 Reason（至少20字符）
□ 抑制注释包含 Approved-By
□ 抑制注释包含 Date (YYYY-MM-DD)
```

**验证方法:**
```bash
# 搜索抑制注释
rg "CLAUDE_SUPPRESS" -A 4 src/
# 手动检查格式
```

**评分规则:**
- 抑制格式不完整 → ❌ FAIL

---

#### N2. 抑制理由充分性
**检查项:**
```
□ Reason 解释为何规则不适用
□ Reason 说明备选方案的劣势
□ Reason 说明对系统的影响
```

**评分规则:**
- 理由不充分（< 20字符） → ❌ FAIL
- 理由模糊（如 "需要"） → ⚠️ WARNING

---

#### N3. 禁止抑制规则检查
**检查项:**
```
□ HARDWARE_DOMAIN_ISOLATION 未被抑制
□ INDUSTRIAL_EMERGENCY_STOP 未被抑制
□ INDUSTRIAL_REALTIME_SAFETY 未被抑制
```

**评分规则:**
- 禁止抑制规则被抑制 → ❌ FAIL

---

## 十一、审计报告生成

### 报告模板

```markdown
# [计划名称] 审计报告

**审计日期:** YYYY-MM-DD
**审计人:** [姓名/AI]
**计划版本:** [版本号]

## 执行摘要
- **总体评分:** ✅ PASS / ⚠️ WARNING / ❌ FAIL
- **高优先级问题:** X 个
- **中优先级问题:** X 个
- **建议实施:** 是/否（需修正问题）

## 问题清单

### 🔴 高优先级问题 (P0 - 必须修正)
1. [问题ID] [问题描述]
   - **位置:** [文件:行号]
   - **违反规则:** [规则名称]
   - **影响:** [安全/性能/正确性]
   - **修正方案:** [具体建议]

### 🟡 中优先级问题 (P1 - 建议修正)
...

### ℹ️ 低优先级问题 (P2 - 可选优化)
...

## 合规性检查结果
- ✅ 架构合规性: X/Y 通过
- ✅ 实时安全性: X/Y 通过
- ✅ 线程安全性: X/Y 通过
- ✅ 测试完备性: X/Y 通过

## 必须补充的测试用例
1. [测试场景]
2. ...

## 实施建议
- **前置条件:** [依赖项]
- **修正工时估算:** X 小时
- **风险评估:** [低/中/高]
- **建议实施时间:** [立即/修正后/暂缓]

## 签核
- [ ] 审计人确认
- [ ] 架构师批准
- [ ] 计划作者确认理解
```

---

## 十二、快速检查脚本

### 自动化检查脚本

```bash
#!/bin/bash
# 文件: scripts/audit-plan.sh
# 用法: ./scripts/audit-plan.sh <plan-file>

PLAN_FILE=$1
REPORT_FILE="docs/audits/$(date +%Y-%m-%d)-$(basename $PLAN_FILE .md)-audit.md"

echo "🔍 开始审计: $PLAN_FILE"
echo ""

# A1: 领域层依赖检查
echo "✓ A1: 领域层依赖检查"
VIOLATIONS=$(rg "#include.*infrastructure|#include.*adapters" src/domain/ 2>/dev/null)
if [ -n "$VIOLATIONS" ]; then
    echo "  ❌ FAIL: 发现领域层依赖违规"
    echo "$VIOLATIONS"
else
    echo "  ✅ PASS"
fi

# B2: 头文件using指令检查
echo "✓ B2: 头文件using指令检查"
VIOLATIONS=$(rg "^using namespace" src/ --glob '*.h' 2>/dev/null)
if [ -n "$VIOLATIONS" ]; then
    echo "  ❌ FAIL: 发现头文件using namespace违规"
    echo "$VIOLATIONS"
else
    echo "  ✅ PASS"
fi

# D1: 动态内存分配检查
echo "✓ D1: 动态内存分配检查"
VIOLATIONS=$(rg "(new |delete |malloc |free )" src/domain/ 2>/dev/null)
if [ -n "$VIOLATIONS" ]; then
    echo "  ⚠️  WARNING: 发现领域层动态分配（检查是否有抑制）"
    echo "$VIOLATIONS"
else
    echo "  ✅ PASS"
fi

# G1: 硬件域隔离检查
echo "✓ G1: 硬件域隔离检查"
VIOLATIONS=$(rg "MultiCard|lnxy" src/domain/ 2>/dev/null)
if [ -n "$VIOLATIONS" ]; then
    echo "  ❌ FAIL: 发现领域层硬件依赖（绝对禁止）"
    echo "$VIOLATIONS"
else
    echo "  ✅ PASS"
fi

echo ""
echo "📝 审计报告已生成: $REPORT_FILE"
```

---

## 附录A: 审计检查点索引

| 检查点 | 优先级 | 规则来源 | 自动化 |
|--------|--------|---------|--------|
| A1 | P0 | HEXAGONAL.md | ✅ |
| A2 | P0 | PORTS_ADAPTERS.md | ✅ |
| A3 | P0 | HEXAGONAL.md | ⚠️ |
| B1 | P0 | NAMESPACE.md | ✅ |
| B2 | P0 | NAMESPACE.md | ✅ |
| C1 | P2 | PORTS_ADAPTERS.md | ❌ |
| C2 | P0 | PORTS_ADAPTERS.md | ⚠️ |
| D1 | P0 | DOMAIN.md | ✅ |
| D2 | P0 | DOMAIN.md | ✅ |
| D3 | P0 | DOMAIN.md | ✅ |
| D4 | P1 | DOMAIN.md | ⚠️ |
| D5 | P0 | DOMAIN.md | ✅ |
| E1 | P0 | INDUSTRIAL.md | ⚠️ |
| E2 | P0 | INDUSTRIAL.md | ✅ |
| E3 | P0 | INDUSTRIAL.md | ✅ |
| F1 | P0 | INDUSTRIAL.md | ⚠️ |
| F2 | P0 | INDUSTRIAL.md | ⚠️ |
| F3 | P1 | INDUSTRIAL.md | ⚠️ |
| G1 | P0 | HARDWARE.md | ✅ |
| G2 | P0 | HARDWARE.md | ⚠️ |
| G3 | P0 | HARDWARE.md | ⚠️ |
| H1 | P1 | 数值稳定性 | ⚠️ |
| H2 | P1 | 数值稳定性 | ⚠️ |
| H3 | P1 | 数值稳定性 | ❌ |
| I1 | P2 | 性能优化 | ⚠️ |
| I2 | P2 | 性能优化 | ⚠️ |
| J1 | P0 | 线程安全 | ⚠️ |
| J2 | P0 | 线程安全 | ⚠️ |
| J3 | P0 | 线程安全 | ⚠️ |
| K1 | P1 | 测试完备性 | ⚠️ |
| K2 | P2 | 测试完备性 | ❌ |
| K3 | P2 | 测试完备性 | ⚠️ |
| L1 | P2 | 测试完备性 | ❌ |
| L2 | P2 | 测试完备性 | ❌ |
| M1 | P2 | 文档完备性 | ❌ |
| M2 | P1 | 文档完备性 | ❌ |
| M3 | P2 | 文档完备性 | ❌ |
| N1 | P0 | EXCEPTIONS.md | ⚠️ |
| N2 | P0 | EXCEPTIONS.md | ❌ |
| N3 | P0 | EXCEPTIONS.md | ⚠️ |

**图例:**
- ✅ 完全自动化
- ⚠️ 半自动化（需人工确认）
- ❌ 需人工审查

---

## 附录B: 常见问题FAQ

### Q1: 如何决定使用CLAUDE_SUPPRESS？
**A:** 仅在以下情况使用：
1. 技术上无法避免违规（如轨迹点容器运行时大小不确定）
2. 备选方案会降低代码安全性或可维护性
3. 理由充分（至少20字符详细说明）
4. 有审批人和日期
5. 规则在 ALLOWED_SUPPRESSIONS 列表中

### Q2: 审计发现问题后如何处理？
**A:**
1. P0问题必须修正才能实施
2. P1问题建议修正，或在计划中添加风险说明
3. P2问题可在后续迭代优化

### Q3: 性能测试标准如何确定？
**A:** 参考以下原则：
- 非实时路径: 启动延迟 < 100ms
- 实时路径: 单次操作 < 1ms（控制周期的10%）
- 大数据集: O(n) 线性复杂度
- 具体数值参考现有系统基准测试

### Q4: 测试覆盖率要求是多少？
**A:**
- 核心业务逻辑: 80%+ 行覆盖率
- 安全关键代码: 100% 分支覆盖率
- 边界条件: 必须有专门测试
- 性能敏感代码: 必须有基准测试

---

**维护责任人:** 架构师
**版本历史:**
- 1.0.0 (2026-01-09): 初始版本

