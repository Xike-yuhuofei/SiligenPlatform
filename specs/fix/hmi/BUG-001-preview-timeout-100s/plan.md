# Implementation Plan: HMI 胶点预览超时窗口调整

**Branch**: `fix/hmi/BUG-001-preview-timeout-300s` | **Date**: 2026-03-29 | **Spec**: `D:\Projects\SiligenSuite\specs\fix\hmi\BUG-001-preview-timeout-100s\spec.md`
**Input**: Feature specification from `D:\Projects\SiligenSuite\specs\fix\hmi\BUG-001-preview-timeout-100s\spec.md`

## Summary

将“打开 DXF 后自动触发的胶点预览”从当前每个关键请求 `15.0s` 的本地 transport wait，改为仅在该自动预览链路上使用 `300.0s` 等待预算。具体上，`PreviewSnapshotWorker` 发起的 `dxf.plan.prepare` 和 `dxf.preview.snapshot` 必须共享新的 `300s` 超时上限；`TcpClient.send_request(...)` 继续作为超时文案唯一来源，使用户最终看到的失败信息自动反映 `300.0s`；同时保持 runtime resync、`dxf.preview.confirm`、`dxf.job.start`、`connect` 等其他入口原有超时不变，避免范围扩散。

## Technical Context

**Language/Version**: Python 3.11（`apps/hmi-app`、`modules/hmi-application`、`pytest`）、PowerShell 7（根级验证入口）  
**Primary Dependencies**: PyQt5 / PyQtWebEngine；`apps/hmi-app` `CommandProtocol` / `TcpClient` / `main_window.py`；`modules/hmi-application` `PreviewSnapshotWorker` / `PreviewSessionOwner`；`shared/contracts/application/commands/dxf.command-set.json`；`pytest`  
**Storage**: Git 跟踪的仓库源码、spec 产物、契约文档与 Python 测试资产；无数据库  
**Testing**: 根级 `.\build.ps1 -Profile Local -Suite all`、`.\test.ps1 -Profile CI -Suite all`；定向 `python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q`；`python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q`  
**Target Platform**: Windows 桌面 HMI，通过 TCP 连接 runtime-gateway  
**Project Type**: Python 桌面 HMI 宿主 + 可复用 HMI application 模块  
**Performance Goals**: DXF 打开后的自动预览链路在 `dxf.plan.prepare` 与 `dxf.preview.snapshot` 阶段都允许最多等待 `300s`；超时失败必须反映新阈值；与本链路无关的请求等待预算保持不变  
**Constraints**: 范围锁定在“打开 DXF 自动触发的胶点预览链路”；不向 `dxf.command-set.json` 新增请求/响应字段；不默认扩大到 runtime resync、`dxf.preview.confirm`、`dxf.job.start`、`dxf.load` 或 `connect`；超时文案必须来自真实等待预算而不是重复硬编码 UI 文案；仅使用 canonical workspace roots  
**Scale/Scope**: 小型 feature slice，覆盖 `apps/hmi-app`、`modules/hmi-application`、feature 契约文档与 Python 单元/UI 回归

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- [x] Branch name is compliant with `<type>/<scope>/<ticket>-<short-desc>`
- [x] Planned paths use canonical workspace roots; no new legacy default paths
- [x] Build/test validation approach is explicit via root entry points (`.\build.ps1`, `.\test.ps1`) plus justified targeted checks (`python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q`, `python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q`)
- [x] Compatibility/fallback behavior is explicit and time-bounded if legacy paths are involved（本特性不引入新的 fallback；不修改 wire contract 默认字段，只在 HMI 自动预览链路上显式放宽等待预算）

## Project Structure

### Documentation (this feature)

```text
D:\Projects\SiligenSuite\specs\fix\hmi\BUG-001-preview-timeout-100s\
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts\
└── tasks.md
```

### Source Code (repository root)

```text
D:\Projects\SiligenSuite\apps\hmi-app\
├── src\hmi_client\client\tcp_client.py
├── src\hmi_client\client\protocol.py
├── src\hmi_client\ui\main_window.py
└── tests\unit\
    ├── test_protocol_preview_gate_contract.py
    └── test_main_window.py

D:\Projects\SiligenSuite\modules\hmi-application\
└── application\hmi_application\preview_session.py

D:\Projects\SiligenSuite\shared\contracts\application\
└── commands\dxf.command-set.json
```

**Structure Decision**: 保持现有 canonical Python 宿主/模块拆分。`CommandProtocol` 与 `TcpClient` 继续作为客户端 transport 边界，`PreviewSnapshotWorker` 继续拥有“打开 DXF 自动预览”这条调用链，`main_window.py` 继续作为用户可见失败消费端，而 `shared/contracts/application` 仍是正式协议源，但本特性不向其新增 timeout 字段，因为本次修改的是本地等待策略，不是 over-the-wire 契约。

## Post-Design Constitution Re-check

- [x] 设计仍只使用 `apps/`、`modules/`、`shared/`、`tests/`、`docs/`、`scripts/`、`config/` canonical roots
- [x] HMI 宿主与 `modules/hmi-application` 的 owner 边界保持不变，没有把 preview owner 重新散落到临时脚本或 legacy 目录
- [x] 验证路径仍以根级入口为主，并补充明确的 HMI Python 定向回归
- [x] 兼容行为被限定为“不扩散超时范围、不新增隐藏 fallback、不修改共享协议 schema”，不存在未记录的隐式兼容分支

## Complexity Tracking

当前无宪章豁免项；本节保持空白。
