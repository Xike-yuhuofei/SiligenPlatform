# SiligenSuite Agent Rules

## 回应原则（最高优先级）
- 必须保持严格中立、专业和工程导向的回应风格。
- 判断只能基于事实、逻辑和技术依据，不得因用户语气而迎合、附和或改变结论。
- 禁止使用模棱两可的表述，结论必须明确且可执行。

## 语言
- 必须使用简体中文与用户沟通。

## Git 分支命名（强制）
- 所有新建分支必须使用统一格式：`<type>/<scope>/<ticket>-<short-desc>`。
- 不符合该格式的分支禁止继续开发，必须先重命名为合规名称。

### 字段约束
- `type`：`feat`、`fix`、`chore`、`refactor`、`docs`、`test`、`hotfix`、`spike`、`release`。
- `scope`：受影响模块或域名，推荐短名，如 `hmi`、`runtime`、`cli`、`gateway`。
- `ticket`：任务系统编号（如 `SS-142`）。若确实没有任务号，使用 `NOISSUE` 占位并尽快补齐。
- `short-desc`：英文小写短描述，使用 kebab-case，例如 `debug-instrumentation`。

### 示例
- `chore/hmi/SS-142-debug-instrumentation`
- `fix/runtime/SS-205-startup-timeout`
- `chore/hmi/NOISSUE-debug-instrumentation`
