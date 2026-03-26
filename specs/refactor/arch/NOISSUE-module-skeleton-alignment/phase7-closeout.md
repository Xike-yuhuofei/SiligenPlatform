# Phase 7 Closeout

更新时间：`2026-03-25`

## 1. 结论

`modules/` 统一骨架对齐的 `Phase 1-7` 已按 `bridge-backed closeout` 口径完成。

该结论的含义是：

- 全部 12 个一级模块已具备统一骨架顶层目录。
- 全部模块已具备 `module.yaml`。
- 历史桥接子树已被显式标记为 `legacy-source bridge`。
- 模块级 `CMakeLists.txt` 已声明统一骨架实现根与 bridge 根。
- 主实现来源的治理口径已切换到统一骨架，bridge 不再作为默认 owner 入口。

该结论不等价于：

- 所有 bridge 目录已被物理清空。
- 所有 fallback CMake、wrapper 和兼容实现已被删除。

## 2. 证据范围

### 2.1 结构证据

- `scripts/migration/validate_workspace_layout.py`
  - 校验 12 个模块存在。
  - 校验 `module.yaml` 存在。
  - 校验 `contracts/`、`domain/`、`services/`、`application/`、`adapters/`、`tests/*`、`examples/` 存在。
  - 校验 `runtime/` 仅允许 `runtime-execution` 使用。
  - 校验 bridge README 存在且包含 `legacy-source bridge` 标记。
  - 校验模块级 `CMakeLists.txt` 已声明 `SILIGEN_TARGET_IMPLEMENTATION_ROOTS` 与 `SILIGEN_BRIDGE_ROOTS`，且 bridge 根未被继续标记为主实现根。

### 2.2 文档证据

- `module-checklist.md`
  - 记录每个模块的骨架完成、边界澄清、实现迁移、构建切换、bridge 降级和 closeout 证据状态。
- `wave-mapping.md`
  - 记录 `Wave 0-4` 的完成状态与 `bridge-backed` 口径。
- 各模块 `README.md`
  - 已补充“统一骨架状态”段落，说明骨架为终态、bridge 为兼容期目录。

### 2.3 构建证据

- `build.ps1 -Profile Local -Suite contracts`
  - 已通过。
  - 未引入新的阻塞性构建错误。

## 3. 当前剩余 bridge 风险

以下目录仍保留兼容桥职责，不应被误判为已完成物理退出：

- `modules/workflow/process-runtime-core`
- `modules/runtime-execution/runtime-host`
- `modules/runtime-execution/device-adapters`
- `modules/runtime-execution/device-contracts`

这些目录当前可以包含：

- fallback `CMakeLists.txt`
- wrapper 或兼容入口
- 尚未完全排空的历史实现

因此，当前 Phase 7 的正确工程表述是：

- 已完成“统一骨架治理切换”
- 未完成“bridge 物理清空”

## 4. 后续建议

如果后续需要继续推进到硬切换终态，建议新开独立阶段处理以下事项：

1. 逐模块清空 bridge 目录中的真实实现，仅保留 tombstone 或删除目录。
2. 将 `workflow`、`runtime-execution` 的 fallback CMake 改为纯转发或彻底移除。
3. 为 bridge drain 引入更强的构建与内容级断言，而不是仅依赖目录与元数据。
