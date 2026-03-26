# samples

`samples/` 是稳定样本、golden cases 与 fixtures 的正式承载根（canonical root）。

## 归位入口

- `samples/README.md` 是稳定样本与 fixtures 的总入口（sample index）。
- `samples/golden/README.md` 是 golden 输入与基准样本的正式入口。
- 领域子目录（如 `samples/dxf/`）仅作为分域说明入口，其承载关系统一回链到本页。

## 正式职责

- 承载可复用、可回归、可审计的样本输入与基准样本。
- 为上游工程链、集成验证与验收基线提供稳定样本事实源。
- 提供 `examples/` 历史样本根的目标收敛面。

## 非职责边界

- 不承载临时实验产物与一次性调试输出。
- 不承载业务 owner 实现代码。
- 不承载运行期自动生成缓存或报告文件。

## 迁移与回流约束

- `examples/` 当前仅作为迁移来源与 redirect/tombstone 承载面，不再作为正式样本 owner。
- 新增长期样本必须优先写入 `samples/`，禁止回流到 `examples/`。
- 样本迁移完成后，应通过根级验证与文档回链（含 `examples/README.md` 退出口径）确认退出条件。
