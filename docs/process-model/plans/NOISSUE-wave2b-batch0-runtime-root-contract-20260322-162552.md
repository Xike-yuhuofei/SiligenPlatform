# NOISSUE Wave 2B Batch 0 - Runtime Root Contract Freeze

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-162552`
- 输入：
  - `docs/process-model/plans/NOISSUE-wave2bB-design-20260322-155730.md`
  - `packages/runtime-host/src/runtime/configuration/WorkspaceAssetPaths.h`

## 1. 冻结目标

冻结 runtime asset-path 解析的 workspace-root 判定合同、兼容窗口和 fail-fast 规则。

## 2. 仓库根判定合同

1. runtime 解析中的 workspace root 必须指向仓库根，而不是任意带 `CMakeLists.txt` 的上层目录。
2. `WORKSPACE.md` 与 `AGENTS.md` 冻结为仓库根判定的必要证据。
3. 模块级 `CMakeLists.txt`、单独 `.git`、单独 `CLAUDE.md` 不得单独作为成功判定 workspace root 的充分条件。
4. 若从任意 cwd 向上扫描后无法同时证明仓库根证据成立，必须 fail-fast。

## 3. 兼容窗口冻结

1. `config/machine_config.ini` 允许作为 read-only inbound compatibility alias。
2. `data/recipes/templates.json` 允许作为 read-only 现场遗留 fallback，但不作为仓内默认资产依赖。
3. 其他旁路一律不纳入兼容窗口：
   - CLI 的 cwd 相对 preview 脚本猜测
   - CLI 的 cwd 相对默认 DXF 样例猜测
   - schema 第二目录猜测
   - upload 目录旧路径猜测
4. 干运行不作为 runtime asset-path 成功的签收依据。

## 4. Fail-Fast 合同

1. 仓库根误判时必须 fail-fast。
2. canonical/compat 配置同时存在但内容不一致时必须 fail-fast。
3. 真实命令仍依赖 cwd 猜测时必须 fail-fast。
4. cutover 关闭后继续显式命中 `config/machine_config.ini` 或 legacy `templates.json` 时必须 fail-fast。

## 5. 后续实现约束

1. `Batch 1` 必须补 `FindWorkspaceRoot()` 的正式回归与 CLI side-path 回归。
2. `Batch 2` 允许保留上述两类只读 fallback，但必须做到显式、可观测、可删除。
3. `Batch 3` 之前不得新增新的 compatibility alias。
4. `Batch 4` 才允许删除 alias/fallback，前提是部署 inventory 已冻结。

## 6. 验收

1. 本批次是设计冻结，不涉及代码回滚。
2. 唯一验收标准是：后续实现者不需要再决定“仓库根怎么判”“哪些旧路径还能保留”“哪些失败必须直接阻断”。
