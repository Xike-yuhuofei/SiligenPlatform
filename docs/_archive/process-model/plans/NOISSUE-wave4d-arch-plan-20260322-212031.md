# NOISSUE Wave 4D Architecture Plan - Interpreter-Gated Engineering Data CLI Verification

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-212031`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave4d-scope-20260322-212031.md`

## 1. 目标结构

```text
active observation doc
  -> canonical preview CLI fixed
  -> launcher gate = python interpreter first

tools/scripts/verify-engineering-data-cli.ps1
  -> resolve python
  -> run required module CLI checks
  -> record optional console script visibility
  -> emit json + md

wave4d-closeout
  -> legacy-exit-pre/
  -> launcher/
  -> intake/
  -> legacy-exit-post/
```

## 2. 组件职责

1. `external-migration-observation.md`
   - 只定义当前正确的 canonical launcher 与观察口径。
2. `verify-engineering-data-cli.ps1`
   - 用统一解释器执行 module CLI 检查并落盘报告。
3. `install-python-deps.ps1`
   - 继续作为唯一官方安装入口，不复制安装逻辑。
4. intake 模板
   - 只记录外部输入元数据，不代替真实外部证据。

## 3. 失败模式与补救

1. 失败模式：helper required checks 失败
   - 补救：定位 Python 环境或 package 安装问题，继续停留在 `Wave 4D`。
2. 失败模式：console script 不可见但 module CLI 可用
   - 补救：在默认模式下记录为附加事实，不单独阻断。
3. 失败模式：活动文档仍写错 canonical CLI 名称
   - 补救：以 `rg` 扫描为 gate，零容忍回流。
