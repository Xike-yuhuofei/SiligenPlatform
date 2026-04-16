# shared 边界冻结 Wave 1

## 1. 目的

本文件只定义 Wave 1 的目标态边界，不表示这些目录已经完成物理迁移。

Wave 1 的任务是冻结 `shared/*` 的职责、禁止承载内容和与现有 `packages/*` 的过渡关系，避免后续迁移时出现双 canonical owner。

## 2. 目标边界

### 2.1 `shared/contracts/application`

- 职责：承载应用层协议定义、命令/查询/模型契约、版本说明和兼容映射。
- 禁止承载：运行逻辑、TCP session/dispatcher、HMI 视图状态、协议实现代码。
- 过渡关系：当前仍由 `packages/application-contracts` 承担事实 owner；Wave 1 仅冻结目标命名，不迁移实现。

### 2.2 `shared/contracts/device`

- 职责：承载设备边界契约，包括 capability、command、event、fault、state、port 等定义。
- 禁止承载：适配器实现、vendor 头、legacy bridge、任何具体硬件访问逻辑。
- 过渡关系：当前仍由 `packages/device-contracts` 承担事实 owner；Wave 1 只冻结契约边界。

### 2.3 `shared/contracts/engineering`

- 职责：承载跨语言工程协议定义，例如 proto、schema、格式映射和版本策略。
- 禁止承载：工程算法实现、CLI、preview 生成、runtime bridge、样例驱动逻辑。
- 过渡关系：当前仍由 `packages/engineering-contracts` 承担事实 owner；Wave 1 不移动 fixtures/cases 到 contracts 本体。

### 2.4 `shared/kernel`

- 职责：承载稳定的基础能力，例如 Result/Error、基础数值和几何类型、字符串/路径/哈希/编码转换。
- 禁止承载：业务倾向明显的类型、追溯业务模型、配置专属模型、观测业务模型。
- 过渡关系：当前 `packages/shared-kernel` 仍是事实 owner；Wave 1 需要先拆账，再谈物理迁移。

### 2.5 `shared/testing`

- 职责：承载工作区级验证运行器、报告模型、known-failure 分类和 workspace validation 编排。
- 禁止承载：场景专属 gate 脚本、integration 场景入口、业务 fixture 数据本体。
- 过渡关系：当前 `packages/test-kit` 仍是事实 owner；Wave 1 只冻结 runner/report model 边界。

### 2.6 `shared/logging`

- 职责：承载稳定日志抽象与公共类型，例如 logging service contract、trace context、formatter、locator。
- 禁止承载：具体 logging 实现、sink 实现、查询/回放/报警记录模型。
- 过渡关系：当前相关职责仍分布在 `packages/shared-kernel` 与 `packages/traceability-observability`；Wave 1 只冻结边界，不做源码合并。

## 3. Wave 1 判断

- `Wave 1` 结论：`Go`
- 含义：可以开始做职责冻结、目录命名冻结、治理文档同步和轻量门禁落地。
- 不含义：不表示可以开始物理迁移 `shared/*` 源码。
- `Wave 1` 物理迁移结论：`No-Go`
- 原因：`shared-kernel`、`shared/logging`、`shared/testing` 相关职责仍存在拆账需求，`contracts` 包中的 fixtures/tests 也还未从目标 contracts 边界中剥离。

## 4. 轻量门禁

Wave 1 的轻量门禁已经接入工作区默认验证链，目的不是判定 `shared/*` 已完成迁移，而是阻止 `shared` 目标边界之外的实现继续扩张。

门禁定义：

1. 门禁入口是 workspace validation 的 `packages` suite。
2. 门禁校验对象是 `packages/application-contracts`、`packages/device-contracts`、`packages/engineering-contracts`、`packages/shared-kernel`、`packages/test-kit`。
3. 门禁规则只检查边界冻结状态，不改变任何 runtime 默认路径、build/test/CI 入口或 canonical owner 事实。
4. 门禁通过表示当前 `shared/*` 目标边界仍可维持冻结状态，不表示实现已迁移。

执行约束：

1. 仅允许新增或保留边界说明、占位目录、门禁说明和迁移锚点。
2. `fixtures/tests`、场景 gate 脚本、logging sink 实现、vendor 头、legacy bridge 不得被门禁误认作 `shared/*` 目标本体。
3. 若门禁失败，优先修正边界扩张或越界引用，不得以修改 canonical 事实的方式消除失败。

## 5. 执行约束

1. `packages/*` 在 Wave 1 仍然是事实 owner，不得同时声明为已迁移 owner。
2. 新增 `shared/*` 内容时，只允许写边界说明、占位目录、治理文档和门禁，不允许迁移业务实现。
3. 任何涉及 `fixtures/tests`、integration 场景脚本、logging sink 实现的内容，Wave 1 不得纳入 `shared/*` 目标本体。
4. 如果后续实现与本文件冲突，以当前 canonical 事实和门禁结果为准，不以目标态文件覆盖现状。
