# src/* 运行/构建/说明引用盘点（脚本/CI/文档）

> 盘点范围：`scripts/`、`.github/`、`docs/`、`README.md`、`CONTRIBUTING*`、仓库治理文档。  
> 说明：仅盘点，不执行修复。`impact_level` 分级中，`高=可执行命令/脚本入口`，`中=构建/运行链路引用`，`低=描述性提及`。

## CI/CD 引用

| file | line | old_ref | replacement_hint | impact_level |
|---|---:|---|---|---|
| `.github/workflows/ci.yml` | 15,18,21 | `cmake -S . -B build` / `cmake --build build --config Debug` / `ctest --test-dir build ...`（无显式 `src/*`，但会执行完整构建图） | `src` 退役阶段建议在 CI 增补 `apps/*` + `modules/*` canonical target 冒烟校验，确认不再依赖 `src/*` 兼容壳 | 中（可执行命令，间接） |

## 本地脚本引用

| file | line | old_ref | replacement_hint | impact_level |
|---|---:|---|---|---|
| `scripts/analysis/arch-validate.ps1` | 11 | `[string]$SourceDir = "src"` | 默认扫描根可切换为 `modules/` + `apps/`（或要求调用方显式传参） | 高（可执行脚本参数） |
| `scripts/analysis/redundancy_scan.py` | 192 | `--source-dir` 默认 `src` | 将默认目录迁移到 canonical 目录集合，保留 `--source-dir` 兼容参数 | 高（可执行脚本参数） |
| `scripts/check-naming-convention.ps1` | 48,73,95,111,154,176 | `Get-ChildItem -Path src -Filter "*.h" -Recurse` | 改为扫描 `apps/`、`modules/`（必要时附带 legacy `src/` 过渡开关） | 高（可执行脚本命令） |
| `scripts/generate_error_codes.py` | 7 | `ERROR_H = ROOT / "src" / "shared" / "types" / "Error.h"` | 若错误码权威定义迁移，改为 canonical 目标路径（建议先引入可配置输入） | 中（可执行脚本命令） |

## 开发文档引用

| file | line | old_ref | replacement_hint | impact_level |
|---|---:|---|---|---|
| `README.md` | 8 | ``src/adapters/hmi`` 已拆出 | 在 README 中给出 HMI 新仓库/新入口，避免继续指向仓内 `src/adapters/hmi` | 中（描述性边界声明） |
| `README.md` | 16,17 | ``src/shared`` / ``src/domain`` / ``src/application`` / ``src/infrastructure`` | 文档主视角逐步切到 `modules/*` + `apps/*`，`src/*` 仅保留 legacy 注解 | 低（描述性提及） |
| `PROJECT_BOUNDARY_RULES.md` | 9 | 允许依赖 ``src/shared``、``src/domain``、``src/application``、``src/infrastructure`` | 边界规则补充 canonical 分层（`modules/*`、`apps/*`）并标注 `src/*` 过渡状态 | 中（规则说明） |
| `docs/MIGRATION_STATUS.md` | 5,6 | 迁入 ``src/`` 且排除 ``src/adapters/hmi`` | 增补“当前 canonical 目录”段，避免读者误判 `src/` 为唯一主入口 | 低（历史状态说明） |
| `docs/04-development/recipe-storage.md` | 10 | schema fallback: ``src/infrastructure/resources/config/files/recipes/schemas/`` | 优先写明 `data/recipes/schemas/` 为主；`src/*` 作为 legacy fallback 单独标注 | 中（运行配置路径） |
| `docs/06-user-guides/recipe-management.md` | 76 | 参数 schema fallback 到 ``src/infrastructure/resources/config/files/recipes/schemas/`` | 同上，明确 fallback 仅兼容用途并给出退役条件 | 中（运行配置路径） |
| `docs/architecture/module-layout.md` | 35 | ``src/`` 仍是可构建实现路径 | 若进入退役阶段，将“可构建主路径”改为 `apps/*` + `modules/*`，`src/*` 降级为 legacy | 中（构建策略说明） |
| `docs/architecture/module-layout.md` | 59 | “不替换现有 `src/*` 构建路径” | 在阶段说明中增加“截至日期/里程碑”避免长期悬空 | 中（构建策略说明） |
| `docs/architecture/final-split-status.md` | 95 | ``src/application/CMakeLists.txt`` 仍拉取 canonical 源 | 迁移完成后把“兼容壳”状态改为“仅保留/已移除”二选一并同步 CI | 中（构建链说明） |
| `docs/architecture/final-split-status.md` | 97 | ``src/infrastructure/CMakeLists.txt`` 仍拉取 canonical 源 | 同上，明确最终落点和退役判定 | 中（构建链说明） |
| `docs/library/00_toolchain.md` | 58 | `python -m pip install -r src/adapters/hmi/requirements.txt` | 该路径当前不存在；应改为 HMI 实际仓库/依赖安装说明 | 高（可执行命令，当前失效） |
| `docs/library/00_toolchain.md` | 71 | HMI 路径 ``src/adapters/hmi/main.py`` | 替换为当前受支持 HMI 启动入口或外部仓库链接 | 中（运行入口说明） |
| `docs/library/00_toolchain.md` | 84 | `python src/adapters/hmi/main.py` | 该命令当前失效；改为实际 HMI 启动命令 | 高（可执行命令，当前失效） |
| `docs/library/04_guides/project-reproduction.md` | 198 | `python -m pip install -r src/adapters/hmi/requirements.txt` | 替换为有效依赖安装来源；避免新人按失效路径执行 | 高（可执行命令，当前失效） |
| `docs/library/04_guides/project-reproduction.md` | 364,382 | `python src/adapters/hmi/main.py` | 替换为现行 HMI 启动入口；若外部仓库，需显式前置条件 | 高（可执行命令，当前失效） |
| `docs/library/04_guides/project-reproduction.md` | 58,283,444 | ``src/infrastructure/resources/config/files/machine_config.ini`` | 明确 `config/machine_config.ini` 与 `src/*` 配置源的主次关系 | 中（运行配置路径） |
| `docs/library/04_guides/project-reproduction.md` | 343 | ``src/infrastructure/resources/config/files/recipes/schemas/default.json`` | 与 `data/recipes/schemas/` 的主备关系统一表述 | 中（运行配置路径） |
| `docs/library/06_reference.md` | 187 | 配置来源链接到 ``src/infrastructure/resources/config/files/machine_config.ini`` | 补充 canonical 配置入口（如 `config/machine_config.ini`） | 中（配置参考路径） |
| `docs/library/error-codes-list.md` | 3 | `Source: src/shared/types/Error.h` | 若错误码定义迁移，需同步生成脚本与文档来源字段 | 中（生成文档来源） |
| `docs/library/error-codes.md` | 4,52 | 错误码权威定义 ``src/shared/types/Error.h`` | 迁移时保持“单一权威来源”不变，仅替换路径 | 中（规范说明） |

## 可忽略归档引用

> 下列为归档/历史文档中的 `src/*` 引用，默认**不纳入强制修复项**；仅在历史追溯时按需处理。

| file | line | old_ref | replacement_hint | impact_level |
|---|---:|---|---|---|
| `docs/_archive/2026-01/PLAN_AUDIT_CHECKLIST.md` | 47 | `rg "#include.*infrastructure|#include.*adapters" src/domain/ ...` | 归档检查脚本示例；不作为现行操作手册 | 忽略（归档，命令示例） |
| `docs/_archive/2026-01/PLAN_AUDIT_CHECKLIST.md` | 748 | `rg "CLAUDE_SUPPRESS" -A 4 src/` | 同上 | 忽略（归档，命令示例） |
| `docs/_archive/2026-01/10-archives/REFACTORING_MIGRATION_MAP.md` | 110 | `Move-Item -Path web/* -Destination src/adapters/web/ -Recurse -Force` | 历史迁移脚本，仅保留参考价值 | 忽略（归档，命令示例） |
| `docs/_archive/2026-01/10-archives/REFACTORING_MIGRATION_MAP.md` | 188 | `add_subdirectory(src/adapters/web)` | 历史构建说明，不应回灌到现行构建策略 | 忽略（归档，构建历史） |
| `docs/_archive/2026-01/10-archives/2025-12/2025-12-09-MultiCard-连接问题解决总结.md` | 77 | `mv src/infrastructure/hardware/MultiCardStub.cpp ...` | 历史故障处理步骤，不作为当前修复指令 | 忽略（归档，命令示例） |
| `docs/_archive/2026-01/10-archives/2025-12/2025-12-09-MultiCard-连接问题解决总结.md` | 81 | `rm -rf build/src/infrastructure/...` | 同上（含破坏性命令） | 忽略（归档，命令示例） |
| `docs/plans/2026-01-18-domain-cleanup-aggressive.md` | 67,300 | 多条 `Remove-Item src/domain/...` | 历史执行计划；仅用于追踪，不直接执行 | 忽略（历史计划，命令示例） |
| `docs/plans/2026-01-18-domain-ports-docs-prune.md` | 70,116 | `git add src/...` | 历史提交步骤，不应作为当前默认流程 | 忽略（历史计划，命令示例） |
| `docs/implementation-notes/code-line-report-2026-03-16.md` | 6 | 明确声明 `src/adapters/cli` / `src/application/cli` 仅作历史参考 | 保持历史属性，不升级为现行约束 | 忽略（历史快照，描述性） |
