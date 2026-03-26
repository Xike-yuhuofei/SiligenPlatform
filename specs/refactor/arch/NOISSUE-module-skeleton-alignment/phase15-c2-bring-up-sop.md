# Phase 15 C2 Bring-Up SOP

## 任务结论

- 已新增 `docs/runtime/hardware-bring-up-smoke-sop.md`，形成首轮真实硬件 bring-up / smoke 现场 SOP。
- SOP 使用的机器配置、vendor 目录、运行脚本和回滚入口均为当前 canonical 路径。
- SOP 已明确区分必须人工确认的安全步骤与可脚本化的 preflight / 证据采集步骤。

## SOP 覆盖面

SOP 当前覆盖以下内容：

- 通电前环境检查
- 机器配置与关键参数核对
- MultiCard vendor 资产检查
- `runtime-service` / `runtime-gateway` dry-run 检查
- 急停、互锁、限位、回零前检查
- 点动 / I/O / 触发 smoke 的执行顺序
- 立即停止条件
- 失败回滚最小动作
- 日志与证据采集清单

## 当前已知前置缺口

- `modules/runtime-execution/adapters/device/vendor/multicard/MultiCard.dll` 当前仍缺失，按 `C1` 结论会阻断真实硬件脚本 preflight。
- `modules/runtime-execution/adapters/device/vendor/multicard/MultiCard.lib` 当前仍缺失；若现场需要重建或替换二进制，不能继续。
- `C3` 观察/记录脚本尚未补齐前，证据采集仍以人工创建 `tests/reports/hardware-smoke/<timestamp>/` 为主。

## 与 C1 / C3 / A4 的衔接

- `C1`：提供 canonical config、vendor 目录和运行脚本 fail-fast 约束；`C2` 基于这些事实定义现场流程。
- `C3`：将把本文的人工证据采集步骤进一步脚本化，并输出标准化结果摘要。
- `A4`：应基于 `C1` 资产缺口、`C2` SOP 完整度和 `C3` 观察入口成熟度做 go/no-go 评审。

## 对 A4 的直接输入

`A4` 评审时至少需要回答：

- `MultiCard.dll` 是否已按 canonical vendor 目录或显式覆盖目录补齐
- 若计划现场重建，`MultiCard.lib` 是否已补齐
- `runtime-service` / `runtime-gateway` dry-run 是否在真实硬件配置下通过
- 现场是否已接受本文定义的停止条件、回滚入口和证据目录规范
- `C3` 脚本是否已可替代或补强本文的人工证据收集步骤

## 当前口径

- `C2` 文档任务已完成。
- 真实上板 smoke 是否可执行，仍取决于 vendor 资产补齐情况和 `A4` 进入门结论。
