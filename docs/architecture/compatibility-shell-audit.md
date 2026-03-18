# Compatibility Shell Audit

更新时间：`2026-03-18`

## 1. 审计范围

- 输入基线：`docs/architecture/canonical-paths.md`
- 目录范围：`apps/`、`packages/`、`control-core/`、`hmi-client/`、`dxf-pipeline/`、`dxf-editor/`
- 入口范围：根级 `build.ps1` / `ci.ps1` / `test.ps1`，以及当前可见的 build / import / include / script 硬编码路径

## 2. 结论摘要

1. Python 应用侧的 canonical 切换已基本完成：`apps/hmi-app`、`apps/dxf-editor-app`、`packages/engineering-data`、`packages/engineering-contracts` 已是默认入口；本次已把仍在执行链路里的 `hmi-client/src` 硬编码测试引用切回 canonical 目录。
2. C++ 运行时链路仍处于 transitional 状态：`apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli` 都还是 wrapper；真实可运行产物与主实现仍主要依附 `control-core` 构建输出或外部旧仓迁移源。
3. `dxf-pipeline` 不是可下线的历史残留，而是“仍被 canonical app 反向依赖的兼容层”：`apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/preview_client.py` 仍直接调用 `dxf_pipeline.cli.generate_preview`。
4. 未发现“同一能力存在两个文档级 canonical 目录”的冲突；但 `apps/hmi-app` / `hmi-client` 与 `apps/dxf-editor-app` / `dxf-editor` 仍暴露相同 Python 包名，属于 legacy mirror 阻塞项，未收口前不能直接删除旧目录。

## 3. 本次已修正的引用

| 类型 | 文件 | 修正内容 | 结果 |
|---|---|---|---|
| 测试硬编码路径 | `packages/application-contracts/tests/test_protocol_compatibility.py` | HMI 源码读取路径从 `hmi-client/src` 改为 `apps/hmi-app/src` | 兼容性测试已以 canonical app 为准 |
| 测试硬编码路径 | `packages/engineering-contracts/tests/test_engineering_contracts.py` | HMI preview consumer 导入路径从 `hmi-client/src` 改为 `apps/hmi-app/src` | 契约测试不再依赖 legacy HMI 目录 |
| legacy build/package 脚本 | `hmi-client/scripts/build.ps1` | 编译检查目标改为 `apps/hmi-app/src` | legacy build 脚本已退化为兼容壳 |
| legacy build/package 脚本 | `dxf-editor/scripts/package.ps1` | 编译检查目标改为 `apps/dxf-editor-app/src` | legacy package 脚本已退化为兼容壳 |
| Windows 绝对旧路径 | `apps/hmi-app/src/hmi_client/ui/main_window.py` | 去除 `D:\\Projects\\Backend_CPP\\uploads\\dxf`，改为工作区内候选目录 + `SILIGEN_DXF_DEFAULT_DIR` 覆盖 | canonical HMI 不再依赖旧仓绝对路径 |
| Windows 绝对旧路径 | `hmi-client/src/hmi_client/ui/main_window.py` | 同步清理 legacy mirror 中的相同绝对路径 | 避免旧目录继续传播历史路径 |
| Windows 绝对旧路径 | `apps/control-cli/run.ps1` / `apps/control-cli/README.md` | 去除对 `D:\\Projects\\Backend_CPP\\src\\adapters\\cli` 的绝对路径硬编码，改为 sibling repo 探测或通用说明 | 保留阻塞说明但不再写死机器路径 |
| 路径级弃用说明 | `hmi-client/README.md` / `dxf-editor/README.md` | 增补“禁止新增从 legacy `src/tests/pyproject.toml` 发起引用”的说明 | 降低旧目录再次被当成 canonical 的风险 |
| legacy CI 壳 | `hmi-client/.github/workflows/ci.yml`、`hmi-client/.github/PULL_REQUEST_TEMPLATE.md` | 删除旧单仓 `hmi-client-ci` workflow 与 PR 模板；CI 统一回到根级 `workspace-validation.yml` | legacy HMI 目录不再暴露独立 CI 入口 |
| legacy CI 壳 | `dxf-editor/.github/workflows/ci.yml`、`dxf-editor/.github/PULL_REQUEST_TEMPLATE.md` | 删除旧单仓 `dxf-editor-ci` workflow 与 PR 模板；CI 统一回到根级 `workspace-validation.yml` | legacy DXF 目录不再暴露独立 CI 入口 |
| 当前协议说明 | `packages/engineering-contracts/*.md` / `packages/editor-contracts/*.md` | 将当前 HMI / 编辑器消费者表述切到 `apps/hmi-app` / `apps/dxf-editor-app` | 当前文档与目录责任对齐 |
| legacy 安装/测试面清理 | `hmi-client/tests/`、`hmi-client/pyproject.toml`、`hmi-client/requirements.txt`、`hmi-client/packaging/` | 删除旧 HMI 安装、测试和打包入口，只保留 README + scripts + 参考源码树 | 阻断从 legacy HMI 目录继续发起 editable install / legacy test |
| legacy 安装/测试面清理 | `dxf-editor/tests/`、`dxf-editor/pyproject.toml`、`dxf-editor/requirements.txt`、`dxf-editor/packaging/` | 删除旧 DXF 编辑器安装、测试和打包入口，只保留 README + scripts + 参考源码树 | 阻断从 legacy DXF 目录继续发起 editable install / legacy test |

## 4. 阻塞项

### 4.1 不可下线

| 阻塞项 | 证据 | 结论 |
|---|---|---|
| `control-core/build/bin/**/siligen_tcp_server.exe` 仍是唯一默认 HIL 可执行入口 | `integration/hardware-in-loop/run_hardware_smoke.py` 默认执行该路径，并以 `control-core` 为 `cwd` | `apps/control-tcp-server` 仍不能脱离 `control-core` 单独运行，`control-core/apps/control-tcp-server` 不可下线 |
| `control-core/build/bin/**/siligen_cli.exe` 仍是唯一工作区内 CLI 产物 | `apps/control-cli/run.ps1 -DryRun` 仍解析到 `control-core/build/bin/Debug/siligen_cli.exe` | `apps/control-cli` 只能继续保留 wrapper，且 CLI 源码迁移未完成 |
| `packages/process-runtime-core` 仍以 `control-core` 为真实实现承载 | `packages/process-runtime-core/CMakeLists.txt` 明确要求 `../../control-core/src/domain/CMakeLists.txt` 存在 | `control-core/src/domain`、`src/application`、`modules/process-core`、`modules/motion-core` 均不可下线 |
| `packages/device-adapters` / `packages/device-contracts` 仍直接 include `control-core/modules/shared-kernel/include` | `packages/device-adapters/CMakeLists.txt`、`packages/device-contracts/CMakeLists.txt` 仍含该 include | `control-core/modules/shared-kernel` 不可下线 |
| `dxf-pipeline` 仍被 canonical HMI 直接调用 | `apps/hmi-app/src/hmi_client/integrations/dxf_pipeline/preview_client.py` 仍解析 sibling `dxf-pipeline` 根目录并执行 `dxf_pipeline.cli.generate_preview` | `dxf-pipeline` 标记为“不可下线” |

### 4.2 显式升级为阻塞的问题

| 问题 | 现状 | 影响 |
|---|---|---|
| Python 包根重复暴露 | `apps/hmi-app` 与 `hmi-client` 都暴露 `hmi_client`；`apps/dxf-editor-app` 与 `dxf-editor` 都暴露 `dxf_editor` | 虽然文档级 canonical 已明确，但 legacy 目录仍能以同名包再次进入 `PYTHONPATH`，会阻碍旧目录下线 |
| CLI 真实迁移源仍在工作区外 | `apps/control-cli` 仍以外部 `Backend_CPP/src/adapters/cli` 作为说明中的可信迁移源 | 无法把 `apps/control-cli` 提升为 effective canonical |
| benchmark / HIL / process-runtime 测试仍直接指向 legacy 构建物 | 典型位置：`dxf-pipeline/benchmarks/trajectory_compare/compare_config.json`、`integration/hardware-in-loop/run_hardware_smoke.py`、`packages/test-kit/src/test_kit/workspace_validation.py` | 说明 `control-core/build` 仍是 source of truth，不能静默切断 |

## 5. Legacy -> Canonical 映射表

| legacy 路径 | canonical 路径 | 当前切换状态 | 当前依据 | 下线结论 |
|---|---|---|---|---|
| `hmi-client` | `apps/hmi-app` | `partial switch` | run/test/build 脚本均已转发或检查 canonical；legacy `src` 仍存在且与 canonical 有差异 | 旧目录脚本可短期保留；`src/tests/pyproject.toml` 进入候删 |
| `dxf-editor` | `apps/dxf-editor-app` | `partial switch` | run/test/package 脚本已指向 canonical；`dxf-editor/src` 与 canonical `src` 116 个共享文件 hash 全一致 | 旧目录脚本可短期保留；`src/tests/pyproject.toml` 进入候删 |
| `dxf-pipeline` | `packages/engineering-data` + `packages/engineering-contracts` | `compatibility shell in active use` | CLI shim、import shim 仍存在；canonical HMI 仍直接调用；契约测试仍验证该兼容层 | `不可下线` |
| `control-core/apps/control-runtime` | `apps/control-runtime` + `packages/runtime-host` | `transitional canonical` | CMake 仅保留 target alias，`apps/control-runtime/run.ps1` 仍只能查找 `control-core/build/bin/**` | 仅能保留兼容 target；不可删除 |
| `control-core/apps/control-tcp-server` | `apps/control-tcp-server` + `packages/transport-gateway` + `packages/runtime-host` | `transitional canonical` | `main.cpp` 仍在 legacy 目录；HIL 默认可执行文件仍在 `control-core/build/bin/**` | `不可下线` |
| `control-core/src/adapters/tcp` | `packages/transport-gateway` + `apps/control-tcp-server` | `compatibility shell` | 仅保留 alias target；`packages/transport-gateway/tests/test_transport_gateway_compatibility.py` 仍校验其存在 | 可在外部 link target 全部收口后再删 |
| `control-core/modules/control-gateway` | `packages/transport-gateway` | `compatibility shell` | 仅保留 legacy target 名称；兼容测试仍显式校验 | 可在 legacy target 名收口后再删 |
| `control-core/src/domain` + `control-core/src/application` | `packages/process-runtime-core` | `blocked` | `packages/process-runtime-core/CMakeLists.txt` 仍直接 include / link legacy 实现 | `不可下线` |
| `control-core/modules/process-core` + `control-core/modules/motion-core` | `packages/process-runtime-core` | `blocked` | process-runtime 真实构建与测试产物仍由 `control-core` 暴露 | `不可下线` |
| `control-core/modules/device-hal` | `packages/device-adapters` + `packages/traceability-observability` | `blocked` | device split 尚未完成，legacy 目录仍承载混合实现 | `不可下线` |
| `control-core/modules/shared-kernel` | `packages/shared-kernel` | `blocked` | `packages/shared-kernel` 尚无真实代码；device packages 仍 include legacy 头文件 | `不可下线` |
| `control-core/config/machine_config.ini` | `config/machine/` | `transitional canonical` | 根级 `config/` 已建，但真实文件仍主要从 `control-core` 复制/安装 | 暂不可删 |
| `control-core/data/recipes` | `data/recipes/` | `transitional canonical` | 根级 `data/` 已建，但真实 recipes 资产仍主要在 legacy 目录 | 暂不可删 |

## 6. 仍在使用的兼容壳清单

| 兼容壳 | 当前调用方 / 触发点 | 保留原因 | 保留级别 |
|---|---|---|---|
| `apps/control-runtime/run.ps1` | 本地 dry-run、开发入口 | 仍需统一对外入口，但仓内没有独立 `control-runtime` exe | `保留，且不可下线` |
| `apps/control-cli/run.ps1` | 本地 dry-run、开发入口 | 当前仅能转发到 `control-core/build/bin/**/siligen_cli.exe`，且源码迁移未完成 | `保留，且不可下线` |
| `apps/control-tcp-server/run.ps1` | 开发入口、HIL 前置准备 | 当前仅能转发到 `control-core` 产物 | `保留，且不可下线` |
| `control-core/apps/control-runtime` | `control-core/apps/CMakeLists.txt` | 仍需暴露 `siligen_control_runtime` / `siligen_control_runtime_app` 兼容 target | `保留` |
| `control-core/modules/control-gateway` | legacy CMake link target、兼容性测试 | 仍需暴露 `siligen_control_gateway` | `保留` |
| `control-core/src/adapters/tcp` | legacy CMake link target、兼容性测试 | 仍需暴露 `siligen_control_gateway_tcp_adapter` / `siligen_tcp_adapter` | `保留` |
| `dxf-pipeline` CLI / import shim | `apps/hmi-app` 预览、`packages/engineering-contracts` 兼容测试、历史 CLI 用户 | canonical HMI 仍直接依赖该 shim；同时承担旧 Python 导入路径兼容 | `保留，且不可下线` |
| `hmi-client/scripts/run.ps1` / `test.ps1` / `build.ps1` | 历史命令入口 | 已全部退化为 canonical 代理；用于留住旧命令入口 | `可保留一个兼容周期` |
| `dxf-editor/scripts/run.ps1` / `test.ps1` / `package.ps1` | 历史命令入口 | 已全部退化为 canonical 代理；用于留住旧命令入口 | `可保留一个兼容周期` |

## 7. 可删除兼容壳候选

| 候选 | 当前判断 | 还需满足的条件 |
|---|---|---|
| `hmi-client/tests/`、`hmi-client/pyproject.toml`、`hmi-client/requirements.txt`、`hmi-client/packaging/` | `已清理` | 本轮已删除，legacy HMI 目录当前保留为 `README + scripts + src/docs/assets/logs` 兼容壳 |
| `dxf-editor/tests/`、`dxf-editor/pyproject.toml`、`dxf-editor/requirements.txt`、`dxf-editor/packaging/` | `已清理` | 本轮已删除，legacy DXF 目录当前保留为 `README + scripts + src/docs/assets/samples` 兼容壳 |
| `hmi-client/.github/workflows/ci.yml`、`hmi-client/.github/PULL_REQUEST_TEMPLATE.md` | `已清理` | 本轮已删除 legacy 本地 CI 壳文件；当前 CI 入口统一为根级 `.github/workflows/workspace-validation.yml` |
| `dxf-editor/.github/workflows/ci.yml`、`dxf-editor/.github/PULL_REQUEST_TEMPLATE.md` | `已清理` | 本轮已删除 legacy 本地 CI 壳文件；当前 CI 入口统一为根级 `.github/workflows/workspace-validation.yml` |
| `hmi-client/src/` | `候删，但先不要直接删` | 仍与 canonical 暴露相同 Python 包名；删除前至少要完成一个兼容周期观察，并确认无外部直连 import |
| `dxf-editor/src/` | `候删，但先不要直接删` | 仍与 canonical 暴露相同 Python 包名；删除前至少要完成一个兼容周期观察，并确认无外部直连 import |
| `hmi-client/scripts/*` | `候删` | 根级 README、CI、开发文档全部切换到 `apps/hmi-app`，且一个兼容周期内无反馈 |
| `dxf-editor/scripts/*` | `候删` | 根级 README、CI、开发文档全部切换到 `apps/dxf-editor-app`，且一个兼容周期内无反馈 |
| `control-core/modules/control-gateway` | `候删` | 外部 CMake / IDE / 文档不再依赖 `siligen_control_gateway` target；同时移除对应兼容性测试断言 |
| `control-core/src/adapters/tcp` | `候删` | 外部 CMake / IDE / 文档不再依赖 `siligen_control_gateway_tcp_adapter` / `siligen_tcp_adapter` |
| `control-core/apps/control-runtime` | `候删但当前不满足前提` | 必须先产出独立 runtime 可执行文件，并让 `apps/control-runtime` 不再回退到 `control-core/build` |

## 8. 禁止新增的旧路径引用规则

1. 新的 build / test / script 入口必须优先引用根级 canonical 路径，不得新增对 `hmi-client/src`、`dxf-editor/src`、`dxf-pipeline/src` 的直接 import，除非该变更明确是在维护兼容壳。
2. 新的 CMake `include_directories()`、`target_include_directories()`、`target_link_libraries()` 不得新增 `../../control-core/...` 形式的硬编码，除非本文件已标记为“不可下线”，并在代码旁注明保留原因。
3. 新的脚本不得再写死 `D:\\Projects\\...` 或工作区外 sibling repo 的绝对 Windows 路径；必须改用工作区相对路径、环境变量或运行时探测。
4. 新的 HMI / editor / engineering 兼容测试必须直接读取 canonical 目录：HMI 用 `apps/hmi-app`，编辑器用 `apps/dxf-editor-app`，工程数据用 `packages/engineering-data`；legacy 目录只允许作为“兼容层仍需存在”的被测对象。
5. 若某个 legacy 路径当前仍是唯一可运行路径，提交中必须显式标注 `不可下线`，并写明切换前置条件；禁止用“后续再清理”代替结论。
6. 任何对 compatibility shell 的保留或扩边界，都必须同时更新本文件；禁止只改脚本或 alias 而不更新审计文档。
7. 文档示例默认使用 repo-relative 路径；只有“历史来源”或“兼容说明”段落允许出现旧路径，且必须显式标注为历史引用。

## 9. 推荐的废弃计划

### Phase A：立即执行

- 继续把自动化、测试、脚本中的 legacy Python 源码引用切到 `apps/hmi-app` / `apps/dxf-editor-app`。
- 禁止再把新功能落到 `dxf-pipeline`、`hmi-client`、`dxf-editor` 的 legacy `src/` 中。
- 把本文件作为 compatibility shell 准入清单；后续新增 legacy alias 时必须先补保留原因。

### Phase B：下一轮收口

- 在本轮已移除 legacy `tests/`、`pyproject.toml`、`requirements.txt`、`packaging/` 的基础上，继续评估 `hmi-client/src/` 与 `dxf-editor/src/` 的下线条件。
- 盘点 `packages/editor-contracts`、`packages/engineering-contracts`、工作区 README 中剩余的 legacy 路径文本，把当前事实统一改到 canonical 路径。

### Phase C：中期前置条件满足后

- 当 `apps/control-runtime`、`apps/control-cli`、`apps/control-tcp-server` 均能直接生成独立产物时，移除对 `control-core/build/bin/**` 的默认回退。
- 当 `packages/process-runtime-core`、`packages/device-adapters`、`packages/device-contracts` 不再 include legacy 头文件时，再评估 `control-core/src/*` 与 `modules/*` 的收口。
- 当 `apps/hmi-app` 直接切到 `engineering_data.cli.generate_preview` 且历史 CLI 用户已迁移后，再评估 `dxf-pipeline` 下线。
