# Quickstart: HMI 胶点预览超时窗口调整

## 1. 目标

在当前 feature 分支上，把“打开 DXF 自动触发的胶点预览”从 `15.0s` 本地等待预算调整为 `100s`，并确保超时失败时用户看到的新阈值与实际等待预算一致，同时不扩大到其他无关请求。

## 2. 关键输入

开始前确认以下资产存在：

1. 规格与设计产物
   `D:\Projects\SiligenSuite\specs\fix\hmi\BUG-001-preview-timeout-100s\spec.md`
   `D:\Projects\SiligenSuite\specs\fix\hmi\BUG-001-preview-timeout-100s\plan.md`
   `D:\Projects\SiligenSuite\specs\fix\hmi\BUG-001-preview-timeout-100s\research.md`
2. 关键实现边界
   `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\client\tcp_client.py`
   `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\client\protocol.py`
   `D:\Projects\SiligenSuite\modules\hmi-application\application\hmi_application\preview_session.py`
   `D:\Projects\SiligenSuite\apps\hmi-app\src\hmi_client\ui\main_window.py`
3. 关键验证入口
   `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py`
   `D:\Projects\SiligenSuite\apps\hmi-app\tests\unit\test_main_window.py`

## 3. 最小实施顺序

1. 先在 protocol 边界引入“调用点显式 timeout override”能力：
   - 只给自动预览链路所需的 `dxf.plan.prepare` / `dxf.preview.snapshot` 开放本地 timeout 覆盖
   - 不把 `100s` 传播到 runtime resync、`dxf.preview.confirm`、`dxf.job.start` 等其他入口
2. 再把 `PreviewSnapshotWorker` 收敛为 `100s + 100s` 的自动预览调用链：
   - `dxf.plan.prepare` 使用 `100s`
   - `dxf.preview.snapshot` 使用 `100s`
3. 最后锁定错误传播与回归：
   - timeout 文案继续由 `TcpClient.send_request(...)` 基于真实 timeout 生成
   - protocol / main window 单元测试明确锁定 `100.0s` 新阈值与范围边界

## 4. 本地验证命令

在仓库根目录 `D:\Projects\SiligenSuite\` 执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile Local -Suite all
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite all
```

面向本特性的快速回归：

```powershell
python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q
python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q
```

说明：本次设计不要求修改 `shared/contracts/application/commands/dxf.command-set.json` 的正式 schema；因此 shared contract 回归不是最小必跑集。

## 5. 通过标准

实施完成后，至少应能证明：

1. 打开有效 DXF 后，自动预览链路中的 prepare 与 snapshot 不会在 `15.0s` 提前 timeout。
2. 任一阶段在 `100s` 内完成时，用户能继续得到预览结果。
3. 任一阶段超过 `100s` 时，用户看到的 timeout 信息明确反映 `100.0s` 或等价 100 秒新阈值。
4. 非 timeout 失败仍保留原始业务语义。
5. runtime resync、`dxf.preview.confirm`、`dxf.job.start`、`connect` 等其他入口没有被被动放大到 `100s`。

## 6. 进入 `speckit-tasks` 的条件

满足以下条件后即可进入 `speckit-tasks`：

1. `plan.md`、`research.md`、`data-model.md`、`contracts\*`、`quickstart.md` 已冻结。
2. “改哪里、不改哪里”的超时边界已经在设计层写清楚。
3. 验证入口已明确到根级脚本与 HMI Python 单元/UI 回归，不再依赖口头说明。
