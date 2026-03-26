# trace-viewer

目标应用面：追溯与证据查看入口。

## 宿主职责

- 只读展示 `modules/trace-diagnostics/` 提供的追溯/诊断事实。
- 读取 `docs/validation/` 与 `tests/` 产出的 evidence 链接与报告索引。
- 不承载 `M10 trace-diagnostics` 的终态 owner 实现。

## 当前事实来源

- `modules/trace-diagnostics/`
- `docs/validation/`
- `tests/`

## 明确禁止

- 在查看入口内重建第二套 `M10 trace-diagnostics` owner 面。
- 在查看入口内生成或改写追溯业务事实。
