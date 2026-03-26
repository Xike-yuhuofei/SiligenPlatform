# Release Process

更新时间：`2026-03-25`

## 1. 适用范围

本文档定义 `SiligenSuite` 根仓的正式版本治理与人工发布流程。

发布判定的唯一正式标准见：

- [docs/runtime/release-readiness-standard.md](/D:/Projects/SS-dispense-align/docs/runtime/release-readiness-standard.md)

本文档只说明执行流程，不降低发布标准。

执行清单见：

- [docs/validation/release-test-checklist.md](/D:/Projects/SS-dispense-align/docs/validation/release-test-checklist.md)

## 2. 发布分类

### 2.1 RC 候选版

- 版本格式：`x.y.z-rc.n`
- 含义：仓内候选版通过最小闭环，可进入受控验证
- 不得外推为正式对外交付

### 2.2 正式发布版

- 版本格式：`x.y.z`
- 含义：满足仓内、仓外与现场门禁
- 必须满足 `release-readiness-standard` 全部正式发布条件

## 3. 仓内必跑门禁

发布前默认门禁入口（本地）：

```powershell
Set-Location <repo-root>
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validation\run-local-validation-gate.ps1
```

门禁脚本会串行执行：

1. 单轨工作区布局校验
2. 根级构建/测试链的预检查
3. freeze/acceptance 相关验证
4. 本地报告固化到 `tests/reports/`

并输出证据到 `tests\reports\local-validation-gate\<timestamp>\`。

在本地门禁通过后，再执行 release 检查：

```powershell
Set-Location <repo-root>
.\release-check.ps1 -Version <version>
```

正式发布版必须显式启用硬件 smoke：

```powershell
Set-Location <repo-root>
.\release-check.ps1 -Version <x.y.z> -IncludeHardwareSmoke
```

`release-check.ps1` 会执行：

1. 检查根仓是否已有可打 tag 的提交
2. 检查工作树是否干净，确保发布边界可审计
3. 检查 `CHANGELOG.md` 是否已有目标版本条目
4. 执行 `legacy-exit-check.ps1`，要求 `finding_count=0`
5. 执行根级 `build.ps1 -Profile CI -Suite all`
6. 执行根级 `test.ps1 -Profile CI -Suite all -FailOnKnownFailure`
7. 检查性能基线证据存在
8. 生成 release evidence 到 `tests\reports\releases\<version>\`
9. 执行以下 app dry-run 并固化输出：
   - `apps\hmi-app\run.ps1 -DryRun -DisableGatewayAutostart`
   - `apps\runtime-gateway\run.ps1 -DryRun`
   - `apps\planner-cli\run.ps1 -DryRun`
   - `apps\runtime-service\run.ps1 -DryRun`

说明：

- 仓库已弃用 GitHub Actions 作为默认门禁，不再要求云端 checks 才能给出发布结论。
- 验收以本地门禁实跑证据为准。
- `release-check.ps1` 只能证明仓内自动化门禁通过，不替代仓外观察、HIL 与真机门禁。

## 4. 正式发布前的附加硬门禁

以下项不因仓内自动化全绿而自动满足：

- 仓外交付物观察：见 [docs/runtime/external-migration-observation.md](/D:/Projects/SS-dispense-align/docs/runtime/external-migration-observation.md)
- 现场/HIL 验收：见 [docs/runtime/field-acceptance.md](/D:/Projects/SS-dispense-align/docs/runtime/field-acceptance.md)
- 部署与回滚准备：见 [docs/runtime/deployment.md](/D:/Projects/SS-dispense-align/docs/runtime/deployment.md)、[docs/runtime/rollback.md](/D:/Projects/SS-dispense-align/docs/runtime/rollback.md)
- 发布判定标准：见 [docs/runtime/release-readiness-standard.md](/D:/Projects/SS-dispense-align/docs/runtime/release-readiness-standard.md)

## 5. DXF 编辑发布口径

- 内建 DXF 编辑器已删除，不再作为发布项。
- `notify / CLI` 编辑器协作协议已废弃，不再作为 release gate。
- 若交付说明涉及 DXF 编辑，统一引用 `docs/runtime/external-dxf-editing.md`。

## 6. 当前已知边界

1. 仓内 release gate 只能证明 canonical 工作区当前通过，不能直接证明仓外迁移已完成。
2. `hardware smoke` 只能证明最小启动闭环，不能替代 HIL 长稳、真机可运行性或工艺签收。
3. 正式发布结论必须同时满足软件、设备和现场运维三个维度，不接受只凭单一维度放行。

## 7. 当前已完成的切换

- `runtime-service`、`runtime-gateway`、`planner-cli` 的默认可执行产物都已切到 canonical control-apps build root。
- `config\machine\machine_config.ini` 与 `data\recipes\` 已是默认配置/数据来源。
- HIL 默认入口已经切到 `tests\e2e\hardware-in-loop\run_hardware_smoke.py` + canonical gateway/runtime 入口。
- legacy gateway/tcp alias 已删除；兼容测试现在只验证 canonical target 仍存在且 alias 不得回流。
- `dxf-pipeline` compat shell、legacy CLI alias 与 root config alias 已从仓内退出；若仓外交付物仍出现这些入口，应直接判为迁移阻断而不是恢复兼容壳。
