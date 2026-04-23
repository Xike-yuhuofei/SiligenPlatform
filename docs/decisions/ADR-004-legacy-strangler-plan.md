# ADR-004 Legacy Strangler Plan

- 状态：`Accepted`
- 日期：`2026-03-18`

## 背景

`SiligenSuite` 已建立 canonical 路径、根级 build/test/CI 与 legacy freeze 规则，但“legacy 目录何时能删、删前要满足什么”此前没有被写成可执行门禁。

当前工作区里，四个目标对象的状态并不相同：

1. `control-core` 仍承载真实产物、真实实现和真实资产，不能按整目录直接切换。
2. `dxf-pipeline` 已退化为 compatibility shell，但 canonical HMI 和 contracts 仍直接使用它。
3. `dxf-editor` 已不再是默认入口，但 legacy `src/dxf_editor` 仍是完整镜像。
4. 旧顶层 `simulation-engine/` 已不存在，但 residual fallback 仍在少数代码路径中保留。

如果不把“删除方式”显式化，最容易出现两类错误：

- 把 freeze 当成 delete，提前删掉仍有真实职责的 legacy 目录；
- 看到 canonical 目录已存在，就误判“真实产物 / 真实脚本 / 真实人工流程”已经全部切过去。

## 决策

自 `2026-03-18` 起，legacy 目录统一按 strangler plan 处理，不采用 big-bang 删除。

### 1. 删除不是目录动作，而是职责归零动作

对 legacy 目录的删除判定，必须以“当前真实职责是否全部迁出”为准，而不是以“目标结构里是否已有 canonical 目录”为准。

特别是：

- `control-core` 必须拆成运行入口、主体源码、shared-kernel/device include、配置、历史管理数据 五类子职责分别 gate。
- `dxf-pipeline` 必须拆成 HMI preview、CLI/import 兼容层、proto/fixture 事实来源三类 gate。
- `dxf-editor` 必须拆成历史脚本、mirror 源码、legacy 文档/样例三类 gate。
- `simulation-engine` 顶层旧目录虽已消失，但 residual fallback 仍要单独 gate，防止回流。

### 2. 采用三档状态分级

legacy 目录统一使用以下状态：

| 状态 | 含义 |
|---|---|
| `可进入删除准备` | 默认入口和 CI 已切走，只剩残留镜像、旧文档、旧脚本或 fallback 待清理。 |
| `仅可冻结` | canonical 已存在，但 live consumer 仍依赖 legacy shell；当前只能冻结，不能删。 |
| `尚不可切换` | legacy 仍是真实产物或真实实现 owner；当前不能删，也不能假装已切换。 |

当前判定如下：

| 目录 | 状态 |
|---|---|
| `control-core` | `尚不可切换` |
| `dxf-editor` | `可进入删除准备` |
| `dxf-pipeline` | `仅可冻结` |
| `simulation-engine`（旧顶层目录） | `可进入删除准备` |

### 3. 删除门禁必须同时覆盖七个面

任何 legacy 目录只有在以下七个面全部过关后，才能进入实际删除：

1. 构建
2. 运行
3. 测试
4. CI
5. 文档
6. 脚本
7. 人工操作切换

其中“人工操作切换”不是软要求，而是 hard gate。

示例：

- `control-core` 若 HIL 仍默认跑 `control-core/build/bin/Debug/siligen_tcp_server.exe`，则不能删。
- `dxf-pipeline` 若 HMI 打开 DXF 时仍默认跳到 `dxf-pipeline/tests/fixtures/...`，则不能删。
- `dxf-editor` 若契约文档仍依赖 `dxf-editor/docs/integration/INTEGRATION.md`，则不能删。

### 4. 删除 gate 必须可执行、可验证、可进 CI

从本 ADR 生效起，不再接受以下表述作为删除结论：

- “后续再清理”
- “理论上应该已经切过去了”
- “canonical 目录已经有了”

必须改为以下形式：

- 明确条件
- 明确验证命令
- 明确 CI 检查方式
- 明确失败时的回滚方式

对应基线文件为：

- `docs/architecture/governance/migration/legacy-deletion-gates.md`

### 5. 删除顺序固定为“小风险先行，大风险后置”

执行顺序固定如下：

1. `dxf-editor`
2. `simulation-engine`（旧顶层 residual cleanup）
3. `dxf-pipeline`
4. `control-core`

原因：

- `dxf-editor` 已切到 canonical app，主要剩镜像与旧文档问题，风险最低。
- `simulation-engine` 顶层旧目录已不存在，主要是 residual fallback 清理。
- `dxf-pipeline` 仍有 active compatibility consumer，需先切消费者。
- `control-core` 风险最高，必须最后做，而且不能整目录一次性切。

## 结果

本 ADR 生效后，团队的统一执行口径如下：

1. `control-core` 当前只允许继续冻结和分职责收口，不允许计划“整目录直接删”。
2. `dxf-pipeline` 当前只允许冻结，不允许因为 canonical `engineering-data` 已存在就直接删除。
3. `dxf-editor` 可以进入删除准备，但必须先解决同名包镜像和 legacy 文档引用。
4. `simulation-engine` 旧顶层目录视为“已基本切走”，后续重点是清 residual fallback、防止旧路径回流。
5. 后续任何 legacy 删除提案，都必须先更新 `docs/architecture/governance/migration/legacy-deletion-gates.md`，再做目录动作。

## 不采纳的方案

### 1. 以目录存在与否作为唯一判断

不采纳原因：

- 顶层旧 `simulation-engine/` 已经不存在，但 residual fallback 仍在；只看目录存在与否会漏掉真实风险。

### 2. 把 freeze 直接当 delete

不采纳原因：

- `dxf-pipeline` 与 `control-core` 都证明：legacy 可以被冻结，但仍有 active chain 在使用，直接删除会断链。

### 3. 对 `control-core` 采用 big-bang 删除

不采纳原因：

- 当前 `control-core` 同时承载运行入口、主体源码、shared-kernel include、配置、recipes，风险面过大，只能分职责 strangler。

## 关联文档

- `docs/architecture/canonical-paths.md`
- `docs/architecture/history/audits/compatibility-shell-audit.md`
- `docs/architecture/governance/migration/legacy-freeze-policy.md`
- `docs/architecture/governance/migration/legacy-deletion-gates.md`
