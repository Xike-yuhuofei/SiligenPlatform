# Premerge Review Report

- branch: `feat/hmi/NOISSUE-dispense-trajectory-preview-v2`
- base: `main`
- baseline synced: `git fetch origin main` (done)
- generated_at: `2026-03-21 23:52:46 +08:00`

## Findings

### P1-1 `SecurityService` 未初始化 `emergency_stop_active_low`，互锁极性进入未定义状态
- 文件:
  - `packages/runtime-host/src/security/SecurityService.cpp:65`
  - `packages/runtime-host/src/security/InterlockMonitor.cpp:183`
- 风险:
  - `InterlockConfig` 新增 `emergency_stop_active_low` 后，`SecurityService::Initialize()` 未赋值该字段，导致 `InterlockMonitor` 读取急停输入时按未定义布尔值判定极性。
  - 在安全链路中这会造成“急停触发/未触发”判定随机化，属于安全功能失真。
- 触发条件:
  - 任何通过 `SecurityService::Initialize()` 初始化互锁监控器的运行路径。
- 建议修复:
  - 在 `SecurityService::Initialize()` 中显式设置 `interlock_cfg.emergency_stop_active_low`（至少给确定默认值，优先从配置源透传）。
  - 为 `InterlockConfig` 增加字段默认初始化，避免未来遗漏赋值再次触发 UB。
  - 补充单测覆盖 `SecurityService -> InterlockMonitor` 极性传递链路。

### P1-2 `emergency_stop_input` 无上界校验，位移运算存在 UB 风险
- 文件:
  - `packages/runtime-host/src/runtime/configuration/InterlockConfigResolver.cpp:128`
  - `packages/runtime-host/src/runtime/configuration/InterlockConfigResolver.cpp:211`
  - `packages/runtime-host/src/security/InterlockMonitor.cpp:181`
- 风险:
  - `InterlockConfigResolver` 仅校验 `emergency_stop_input >= 0`，未限制到硬件位宽。
  - `InterlockMonitor` 直接执行 `1L << emergency_input`，当输入位超出 `long` 位宽时会触发未定义行为。
- 触发条件:
  - 配置误填 `emergency_stop_input` 为超范围值（例如 >= 32/64，取决于平台）。
- 建议修复:
  - 在 resolver 层将 `emergency_stop_input` 限制在有效 DI 位范围（与硬件组定义一致），非法时回退并告警。
  - 在 monitor 层增加防御式边界检查，超范围时直接进入 fail-safe（按触发处理）并输出高优先级日志。

### P2-1 冗余仓库仅在“文件打不开”时尝试 `.bak` 恢复，JSON 损坏场景未覆盖
- 文件:
  - `packages/process-runtime-core/src/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.cpp:57`
  - `packages/process-runtime-core/src/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.cpp:83`
- 风险:
  - 当前恢复逻辑仅覆盖 open 失败；当主文件可打开但 JSON 损坏时直接返回 `JSON_PARSE_ERROR`，不会走 `.bak` 恢复。
  - 异常掉电或磁盘写入中断会把系统留在“可读但不可解析”状态，恢复能力不足。
- 触发条件:
  - 主存储文件内容损坏但文件句柄可正常打开。
- 建议修复:
  - 在 parse failure 分支追加 `.bak` 读取与替换回退。
  - 增加“主文件 JSON 损坏 + 备份正常”的回归测试。

## Open Questions

1. 当前发布路径是否仍会使用 `SecurityService` 初始化互锁监控器（而非 runtime-host 组合根路径）？如果会，P1-1 需作为阻断项处理。
2. `emergency_stop_input` 的合法范围是否固定为 `X0~X15`（与 `MC_GetDiRaw` 组一致）？需要在配置规范中固化。

## Residual Risks

- 本次 diff 规模较大（跨 C++/Python/配置/文档），未执行全量构建与联机硬件回归，仍存在集成层残余风险。
- `dxf` 工作流状态机新增路径较多，现有测试主要覆盖单点行为，跨模块时序竞争仍需系统级验证。

## 建议下一步

1. 先修复 P1-1 / P1-2，再进入 QA 回归（至少覆盖急停极性与输入位越界场景）。
2. 补齐 P2-1 的 `.bak` parse-fallback 与回归用例后，再推进合并。
