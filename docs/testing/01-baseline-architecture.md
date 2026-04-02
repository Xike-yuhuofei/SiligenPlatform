# 01 Baseline Architecture

## 1. Purpose

### 1.1 文档目的
本文档冻结 `SiligenPlatform` 项目的分层测试体系总纲，用于统一：
- 测试目标
- 分层模型
- 各层职责与非职责
- 项目范围内的核心术语
- 模块内测试与仓库级测试的边界原则

### 1.2 适用范围
本文档适用于：
- `modules/` 下业务 owner 模块
- `apps/runtime-service`
- 根级 `tests/` 目录承载的 integration / e2e / performance 验证面
- 根级 `build.ps1`、`test.ps1`、`ci.ps1` 驱动的测试执行体系

### 1.3 非适用范围
本文档不直接规定：
- 单个测试用例的具体写法
- 具体测试数据的版本生命周期
- Agent 的操作流程
- 每阶段实施排期

这些内容由后续文档负责。

---

## 2. Testing Goals

### 2.1 项目级测试目标
本项目测试体系的目标不是单纯追求“测试数量”或“覆盖率数字”，而是建立一套能覆盖以下风险面的分层自动化验证体系：
- 功能正确性
- 状态与时序正确性
- 协议兼容性
- 故障注入与恢复路径
- 性能与长稳风险
- 观测一致性与证据可诊断性

### 2.2 自动化覆盖目标
测试体系应尽可能将原本依赖人工联机验证的风险前移到：
- 模块内小测试
- 模块契约测试
- 仓库级集成测试
- simulated-line E2E

HIL 与真实设备验证只承担模拟环境无法充分证明的风险。

### 2.3 风险优先级目标
测试建设应优先覆盖以下高风险带：
1. 工艺语义到执行载荷的传递链
2. runtime 状态机与恢复路径
3. 宿主装配与模式切换
4. 协议兼容与事件时序
5. 证据产出与复盘能力

### 2.4 证据可判定目标
每一类正式测试都必须尽量输出可机器判定、可归档、可复盘的结构化证据，而不是仅依赖人工阅读日志。

---

## 3. Core Principles

### 3.1 分层优先于堆叠
优先通过正确的测试分层来解决问题，而不是不断叠加更高层、更重、更慢的测试。

### 3.2 契约优先于端到端补救
模块之间的输入/输出契约应尽可能在模块内或仓库级 integration 层证明；E2E 不是补齐所有契约漂移问题的兜底层。

### 3.3 模拟优先于真机扩张
只要问题可以在 simulated-line 证明，就不应直接转移到 HIL 或真实设备上解决。

### 3.4 证据优先于人工解释
测试失败时，应优先输出 summary、timeline、artifact dump、report 等结构化证据，而不是依赖人工解释日志。

### 3.5 模块 owner 对模块测试负责
模块内测试由对应模块 owner 负责；仓库级 tests 只承载跨 owner 的验证，不回写或替代业务 owner 责任。

---

## 4. Layered Testing Model

### 4.1 L0 结构与治理门禁
目标：
- 保证仓库结构、入口、边界、规范仍处于受控状态

典型内容：
- workspace layout 检查
- canonical root / canonical subdirectory 检查
- legacy-exit / bridge / formal contract gate
- 根级入口与文档集合检查

产出：
- 结构化 gate 结果
- 最小治理报告

### 4.2 L1 模块内小测试
目标：
- 快速、确定性地验证纯逻辑、纯算法、纯映射

典型内容：
- 单元测试
- 数值规则测试
- 序列化/反序列化小测试
- 小粒度状态迁移测试

要求：
- 尽量不依赖真实网络、真实硬件、真实时间

### 4.3 L2 模块契约测试
目标：
- 冻结模块对外行为与产物语义

典型内容：
- 输入契约
- 输出契约
- 错误契约
- 观测契约
- golden / compatibility / schema-level 验证

### 4.4 L3 跨模块集成测试
目标：
- 验证多个模块之间的拼接、边界、协作与共享事实样本

典型内容：
- 上游链路集成
- 执行链路集成
- protocol-compatibility
- service/runtime 装配验证

### 4.5 L4 Simulated-Line E2E
目标：
- 在可控模拟环境中验证 acceptance 场景与故障路径

典型内容：
- success
- block
- rollback
- recovery
- archive

要求：
- 具备 fault injector
- 具备 event recorder
- 输出正式 evidence bundle

### 4.6 L5 HIL
目标：
- 验证模拟环境无法充分证明的硬件相关风险

典型内容：
- hardware smoke
- closed-loop
- case matrix
- 关键安全链与真实反馈链

要求：
- HIL 不是测试体系主战场
- 只覆盖高价值、低可替代的风险点

### 4.7 各层输入、输出、判定物
| 层级 | 输入 | 输出 | 主判定物 |
|---|---|---|---|
| L0 | 仓库结构、入口、规范 | gate result | 结构化检查结果 |
| L1 | 小粒度逻辑输入 | 逻辑输出 | assertion |
| L2 | 模块输入/契约样本 | 模块产物 | golden / schema / contract |
| L3 | 跨模块样本 | 集成结果 | scenario result / contract evidence |
| L4 | acceptance 场景 + 模拟故障 | E2E 结果 | evidence bundle |
| L5 | 硬件/半实物环境 | HIL 结果 | HIL evidence bundle |

---

## 5. What Each Layer Must Solve

### 5.1 正确性问题归属
- 算法与映射问题：优先 L1/L2
- 模块产物语义问题：优先 L2
- 跨模块拼接问题：优先 L3
- acceptance 路径问题：优先 L4
- 真硬件闭环问题：优先 L5

### 5.2 状态与时序问题归属
- 小粒度状态机逻辑：L1/L2
- 多组件事件序列：L3/L4
- 真实反馈闭环：L5

### 5.3 协议兼容问题归属
- 协议对象/字段层兼容：L2/L3
- 真实宿主与 runtime 行为：L3/L4
- 硬件侧时序兼容：L5

### 5.4 性能与长稳问题归属
- 局部热点性能：L1/L2/L3
- 场景级性能：L4 / performance
- 长稳 soak：L4 / L5

### 5.5 观测一致性问题归属
- 模块级观测契约：L2
- 跨组件状态投影：L3/L4
- 真实设备观测一致性：L5

---

## 6. What Each Layer Must Not Solve

### 6.1 不允许把模块逻辑问题推给 E2E
若错误可在模块内被精确断言，则不得仅通过 E2E 暴露。

### 6.2 不允许把契约漂移问题推给 HIL
契约问题必须尽量在模块内或 integration 层发现；不得依赖 HIL 兜底。

### 6.3 不允许用 GUI 自动化承担主正确性判断
GUI 自动化只负责用户可见状态投影与交互约束，不承担主正确性证明。

### 6.4 不允许把性能问题仅靠真机复现
性能基线应尽量在固定样本与固定 profile 下建立，而不是只靠真实设备偶发复现。

---

## 7. Testing Pyramid Interpretation For This Project

### 7.1 本项目中的金字塔结构
推荐的数量与投资重心分布：
- 大量 L1/L2
- 中量 L3
- 少量 L4
- 极少量 L5

### 7.2 数量配比建议
作为长期目标，可参考以下近似分布：
- L1：35%~45%
- L2：20%~30%
- L3：15%~20%
- L4：5%~10%
- L5：1%~5%

### 7.3 为什么不追求“大量端到端”
- 运行慢
- 脆弱
- 定位成本高
- 难以形成高判定力资产

### 7.4 为什么 simulated-line 是主战场
因为它兼顾：
- 自动化程度
- 场景覆盖
- 故障注入能力
- 证据产出质量

---

## 8. Project-Wide Terminology

### 8.1 suite
按验证面组织的正式测试集合，如 contracts、e2e、protocol-compatibility、performance。

### 8.2 label
按运行成本、风险或语义维度对测试进行正交标记，如 small、medium、hil、perf。

### 8.3 fixture
测试输入资产，包括样本文件、配置、协议包、场景数据等。

### 8.4 golden
可作为预期标准输出或基线的参考结果。

### 8.5 evidence bundle
测试运行后输出的正式证据集合，至少包含 summary、timeline、artifacts、report。

### 8.6 baseline
被明确冻结并作为后续回归判定依据的参考状态。

### 8.7 smoke / regression / soak / stress
- smoke：最小可跑通验证
- regression：防止已知功能退化
- soak：长时间持续运行验证
- stress：高负载/边界资源验证

### 8.8 success / block / rollback / recovery / archive
仓库 acceptance 场景的五大主类。

---

## 9. Mapping To Repository Execution Surface

### 9.1 根级入口与测试层的对应关系
- `build.ps1`：构建入口
- `test.ps1`：正式测试入口
- `ci.ps1`：仓库治理与测试编排入口

### 9.2 模块内 tests 与仓库级 tests 的对应关系
- 模块内 `tests/`：L1/L2 主承载面
- 根级 `tests/integration`：L3 主承载面
- 根级 `tests/e2e`：L4/L5 承载面
- 根级 `tests/performance`：L3/L4/L5 的性能/长稳验证承载面

### 9.3 integration / e2e / performance 的位置
正式仓库级验证面只允许在规定目录中承载对应类别验证。

---

## 10. Architecture-Level Anti-Patterns

### 10.1 只增 E2E 不补契约
### 10.2 只看日志不建 golden
### 10.3 只在真机上验证
### 10.4 仓库级 tests 侵入模块 owner
### 10.5 测试代码无证据输出

---

## 11. Normative Rules

### 11.1 MUST
- MUST 使用分层模型组织测试建设
- MUST 优先在低层解决可低层证明的问题
- MUST 为正式测试输出可归档证据
- MUST 明确模块 owner 与仓库级 owner 边界

### 11.2 SHOULD
- SHOULD 优先建设 L1/L2/L3，再扩展 L4/L5
- SHOULD 优先建设契约、golden、fault injection
- SHOULD 让 simulated-line 成为 acceptance 主战场

### 11.3 MAY
- MAY 在必要时引入额外 profile、额外 scenario 子类
- MAY 为特殊模块添加专用 label

### 11.4 MUST NOT
- MUST NOT 用 HIL 替代模块契约测试
- MUST NOT 让 GUI 自动化成为主正确性判定层
- MUST NOT 在随机目录新增正式测试入口

---

## 12. References

### 12.1 相关基准文档
- `02-baseline-directory-and-ownership.md`
- `03-baseline-module-test-matrix.md`
- `05-baseline-suite-label-ci.md`
- `06-baseline-simulated-line-and-hil.md`

### 12.2 仓库根入口
- `build.ps1`
- `test.ps1`
- `ci.ps1`

### 12.3 后续治理文档索引
见 `docs/testing/` 目录。
