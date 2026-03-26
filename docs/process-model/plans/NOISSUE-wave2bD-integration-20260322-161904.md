# NOISSUE Wave 2B-D Integration

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-161904`
- 输入文件：
  - `docs/process-model/plans/NOISSUE-wave2bA-design-20260322-155701.md`
  - `docs/process-model/plans/NOISSUE-wave2bB-design-20260322-155730.md`
  - `docs/process-model/plans/NOISSUE-wave2bC-design-20260322-155805.md`
- 说明：本文件只做设计整合、批次划分、串并行裁决、进入/退出条件和回滚单元定义；不包含实现，不包含新的 build/test 执行。

## 1. 总体裁决

1. `Wave 2B Design Freeze = Go`
2. `Wave 2B Physical Cutover = No-Go`
3. 当前允许进入的是“实施前置批次”和“桥接批次”，不允许直接进入 root source-root flip、runtime 默认资产路径 flip 或 residual ledger 清零。

## 2. 为什么当前仍然不是 Physical Cutover Go

1. `2B-A` 已证明 root source-root cutover 不是单文件改动，而是 `CMakeLists.txt`、`tests/CMakeLists.txt`、`tools/build/build-validation.ps1` 组成的原子合同单元；当前目标 source root 终态和 selector 合同仍未冻结。
2. `2B-B` 已证明 runtime asset-path 当前不是真正单点解析；`FindWorkspaceRoot()`、CLI side paths、compat fallback inventory 都还没有进入可实施状态。
3. `2B-C` 已证明 residual ledger 当前只适合“防扩散”，还不能直接作为 payoff 施工单；recipes/logging canonical owner 与 third-party consumer-owned 机制都尚未冻结。
4. 结论：A/B/C 三条分析流都已完成，但它们输出的是“切换前必须先补的前置条件”，不是“现在就能翻转”的许可。

## 3. 实施批次划分

### Batch 0：前置冻结批次

- 性质：必须串行，且优先于所有真实桥接实现
- 目标：冻结后续所有切换动作必须共享的合同与 owner
- 子任务：
  1. 冻结目标 superbuild source-root 终态与 selector 合同
  2. 冻结 `FindWorkspaceRoot()` 的“仓库根判定合同”
  3. 冻结 recipes persistence/serializer canonical owner
  4. 冻结 diagnostics logging canonical owner
  5. 冻结 third-party consumer-owned 注册机制
  6. 冻结 residual ledger 的细粒度 schema 与拆账口径
- 进入条件：
  1. A/B/C 设计文档已存在且状态均为 `Done`
  2. 当前 `Wave 2B prep` 回归基线仍是已知最新绿灯基线
- 退出条件：
  1. 上述 6 个合同/owner 全部形成明确文档结论
  2. 不再存在“实施时再决定 owner/source-root/selector 语义”的开放项
- 回滚单元：
  1. 设计文档级回滚，无代码回滚
  2. 若冻结结论互相冲突，则直接退回到本批次重新裁决，不进入后续批次
- 验证命令：
  1. 无新的 build/test 命令
  2. 以文档存在性、结论完整性和主线程一致性检查为准

### Batch 1：基线门禁与可观测性批次

- 性质：可以并行实现，但只能在 Batch 0 完成后启动
- 目标：补齐切换前必须存在的 selector、provenance、resolver、ledger 可观测能力
- 子任务：
  1. 为 root build/test 引入单一 source-root selector，并让 build/test provenance 可观测
  2. 为 runtime resolver 建立稳定的仓库根判定与 fail-fast 规则
  3. 为 CLI side paths 增加正式回归入口
  4. 为 residual ledger 增加细粒度 owner/type/must-zero-before-cutover 设计对应的实现骨架
- 并行裁决：
  1. `1A` root selector/provenance
  2. `1B` runtime resolver/CLI path gate
  3. `1C` residual ledger schema/gate
  4. `1A/1B/1C` 可并行，只要写集隔离
- 进入条件：
  1. Batch 0 全部退出
  2. 写集边界已冻结
- 退出条件：
  1. 新增门禁与可观测性在 bridge 态下通过
  2. 对外公开合同不变：`build.ps1` / `test.ps1` 参数面、artifact 根、`integration/reports` 根不变
- 回滚单元：
  1. `1A` root selector/provenance 单元
  2. `1B` runtime resolver/CLI path 单元
  3. `1C` residual ledger schema 单元
  4. 每个单元独立回滚，不得交叉回退
- 验证命令：
  1. `.\build.ps1 -Profile CI -Suite apps`
  2. `.\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2b-batch1\packages -FailOnKnownFailure`
  3. `.\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2b-batch1\apps -FailOnKnownFailure`
  4. 新增 CLI / resolver / residual gate 对应单测与静态检查命令

### Batch 2：桥接实现批次

- 性质：部分可并行，但必须晚于 Batch 1 全绿
- 目标：实现“先桥接、不翻转”的稳定过渡态
- 子任务：
  1. root build/test/source-root 全部读取同一 selector，但 selector 默认值仍指向当前 workspace root
  2. runtime 资产路径全部进入统一 resolver，compat fallback 保持只读并可观测
  3. residual ledger 从粗条目升级为可派工语义，但仍不删 allowlist
- 并行裁决：
  1. `2A` root bridge
  2. `2B` runtime bridge
  3. `2C` residual ledger split
  4. `2A/2B/2C` 可以并行推进，但合并前必须统一 review
- 进入条件：
  1. Batch 1 全部通过
  2. 回滚单元已在代码层明确
- 退出条件：
  1. 仍未翻转默认值，但所有关键入口都已经切到单一桥接机制
  2. 桥接态下根级 suites 全绿
- 回滚单元：
  1. root bridge 单元
  2. runtime bridge 单元
  3. residual split 单元
- 验证命令：
  1. `.\build.ps1 -Profile CI -Suite apps`
  2. `.\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2b-batch2\apps -FailOnKnownFailure`
  3. `.\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2b-batch2\packages -FailOnKnownFailure`
  4. `.\test.ps1 -Profile CI -Suite integration,protocol-compatibility,simulation -ReportDir integration\reports\verify\wave2b-batch2\extended -FailOnKnownFailure`

### Batch 3：受控翻转批次

- 性质：必须串行，不允许并行
- 目标：执行真正的 flip，但一次只翻一个合同家族
- 子任务顺序：
  1. `3A` root source-root flip
  2. `3B` runtime 默认资产路径 flip
  3. `3C` residual payoff 中切换前必须清零的 live debt flip
- 进入条件：
  1. Batch 2 桥接态全绿
  2. 回滚单元已演练或至少文档化完成
  3. `3A/3B/3C` 不存在未冻结 owner/open question
- 退出条件：
  1. 每次只翻一类合同，并通过对应根级 suites
  2. 任一 flip 失败立即回滚，不允许“先带着失败继续推进下一类”
- 回滚单元：
  1. `3A` root superbuild source-root 合同单元
  2. `3B` runtime asset-path 合同单元
  3. `3C` residual owner/payoff 合同单元
- 验证命令：
  1. `.\build.ps1 -Profile CI -Suite apps`
  2. `.\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2b-batch3\apps -FailOnKnownFailure`
  3. `.\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2b-batch3\packages -FailOnKnownFailure`
  4. `.\test.ps1 -Profile CI -Suite integration,protocol-compatibility,simulation -ReportDir integration\reports\verify\wave2b-batch3\extended -FailOnKnownFailure`
  5. runtime 真实配置读取、CLI 非仓库根 cwd、simulated-line、相关静态零命中 gate

### Batch 4：删桥与封板批次

- 性质：可以局部并行，但必须晚于 Batch 3 全部通过
- 目标：删除 bridge/fallback、收紧 gate、清理历史证据点
- 子任务：
  1. 删除 source-root bridge 和旧 provenance 兼容逻辑
  2. 删除 `config/machine_config.ini` alias 与 `templates.json` 只读 fallback
  3. 收紧 residual ledger 到最终目标，删除已完成 family 的 allowlist
  4. 归并文档 residual 到单一证据点
- 进入条件：
  1. Batch 3 全部通过
  2. 现场/部署 inventory 证明 bridge 可以移除
- 退出条件：
  1. 新结构成为唯一有效路径
  2. legacy compatibility 不再参与默认行为
- 回滚单元：
  1. 每个删除波次独立回滚，不允许把多类兼容层一起清除后再统一补救
- 验证命令：
  1. 继承 Batch 3 全量根级命令
  2. 新增旧路径零命中静态 gate

## 4. 串并行裁决

1. `Batch 0` 必须串行。
2. `Batch 1` 可并行拆成 `1A/1B/1C`。
3. `Batch 2` 可并行拆成 `2A/2B/2C`，但合并前必须统一 review。
4. `Batch 3` 必须严格串行，且顺序固定为 `3A -> 3B -> 3C`。
5. `Batch 4` 可局部并行，但要按 compatibility family 拆开写集。

## 5. 当前 No-Go 条件

1. 目标 superbuild source-root 终态未冻结。
2. `FindWorkspaceRoot()` 的仓库根判定合同未冻结。
3. recipes/logging canonical owner 未冻结。
4. third-party consumer-owned 机制未冻结。
5. residual ledger 仍是粗粒度 schema，无法直接作为 payoff 施工单。
6. CLI side path 正式回归缺失；此时翻 runtime 默认资产路径属于盲改。
7. 任何试图跳过 Batch 0/1，直接进入 Batch 3 flip 的实施都属于 `No-Go`。

## 6. 当前允许开始的下一步

1. 进入 `Batch 0`，先把 source-root 终态、仓库根判定合同、recipes/logging owner、third-party 注入机制、ledger schema 一次性冻结。
2. 在 Batch 0 完成后，启动 `Batch 1` 的三个并行实现流。
3. 在 Batch 1 全绿前，不允许启动任何 flip 类改动。

## 7. 最终结论

1. 当前设计整合已经完成，A/B/C 三条分析流足以支撑后续实施排程。
2. 当前不是“B 缺失文件导致整合失败”，而是“B 已完成，且它明确给出了阻断 flip 的前置条件”。
3. 正确的主线程裁决不是“只能进入极小子集且其余全阻断”，而是：
   - 设计冻结：`Go`
   - 进入实施前置批次：`Go`
   - 直接物理切换：`No-Go`
