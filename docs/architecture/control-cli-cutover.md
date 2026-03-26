# Control CLI Cutover

更新时间：`2026-03-19`

## 1. 目标与结果

本轮目标是把仍依赖 legacy fallback 的 CLI 命令迁入 canonical CLI，使 `apps/control-cli` 成为唯一默认入口，并删除 `apps/control-cli/run.ps1` 对 `-UseLegacyFallback` 的显式依赖。

本轮结果：

1. `apps/control-cli` 现在承载连接调试、运动、点胶、DXF、recipe 的真实命令实现。
2. `apps/control-cli/run.ps1` 现在只解析 canonical `siligen_cli.exe`，不再探测 `control-core/build/bin/**/siligen_cli.exe`。
3. `control-core/build/bin/**/siligen_cli.exe` 已不再是默认入口，也不再是显式 fallback 入口。

## 2. 当前哪些 CLI 命令仍走 legacy fallback

当前结论：`无`。

本次 cutover 后，以下命令全部直接在 canonical CLI 中执行，不再依赖 legacy fallback：

- `bootstrap-check`
- `connect`
- `disconnect`
- `status`
- `home`
- `jog`
- `move`
- `stop-all`
- `estop`
- `dispenser start`
- `dispenser purge`
- `dispenser stop`
- `dispenser pause`
- `dispenser resume`
- `supply open`
- `supply close`
- `dxf-plan`
- `dxf-dispense`
- `dxf-preview`
- `dxf-augment`
- `recipe create`
- `recipe update`
- `recipe draft`
- `recipe draft-update`
- `recipe publish`
- `recipe list`
- `recipe get`
- `recipe versions`
- `recipe archive`
- `recipe version-create`
- `recipe compare`
- `recipe rollback`
- `recipe activate`
- `recipe audit`
- `recipe export`
- `recipe import`

## 3. 命令的 canonical 承载面

| 命令面 | 根级 CLI 承载文件 | canonical use case / 运行时承载面 |
|---|---|---|
| 启动与命令分发 | `apps/control-cli/main.cpp`、`apps/control-cli/CommandLineParser.*`、`apps/control-cli/CommandHandlers.cpp` | `packages/runtime-host/src/ContainerBootstrap.h` + `packages/runtime-host/src/container/ApplicationContainer.*` |
| 连接调试 | `apps/control-cli/CommandHandlers.Connection.cpp` | `InitializeSystemUseCase`、`MotionInitializationUseCase`、`MotionMonitoringUseCase`、`ValveQueryUseCase` |
| 运动 | `apps/control-cli/CommandHandlers.Motion.cpp` | `HomeAxesUseCase`、`ManualMotionControlUseCase`、`MotionSafetyUseCase`、`EmergencyStopUseCase` |
| 点胶 / 供胶 | `apps/control-cli/CommandHandlers.Dispensing.cpp` | `ValveCommandUseCase`、`MotionMonitoringUseCase` |
| DXF | `apps/control-cli/CommandHandlers.Dxf.cpp` | `PlanningUseCase`、`DispensingExecutionUseCase`、`ContourAugmenterAdapter`、`HomeAxesUseCase` |
| recipe | `apps/control-cli/CommandHandlers.Recipe.cpp` | `CreateRecipeUseCase`、`UpdateRecipeUseCase`、`CreateDraftVersionUseCase`、`UpdateDraftVersionUseCase`、`RecipeCommandUseCase`、`RecipeQueryUseCase`、`CreateVersionFromPublishedUseCase`、`CompareRecipeVersionsUseCase`、`ExportRecipeBundlePayloadUseCase`、`ImportRecipeBundlePayloadUseCase` |

约束落实：

- 不再把新命令落回 `control-core`。
- 若后续需要新增 CLI 能力，真实实现必须继续加在 `apps/control-cli`，依赖 canonical package/use case。

## 4. `run.ps1` 去掉 fallback 后的默认行为

`apps/control-cli/run.ps1` 当前行为：

1. 按 `SILIGEN_CONTROL_APPS_BUILD_ROOT` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build` -> `D:\Projects\SiligenSuite\build\control-apps` 解析 build root。
2. 只查找 canonical `siligen_cli.exe`。
3. `-DryRun` 找到产物时输出：
   - `control-cli target: <canonical path>`
   - `source: canonical`
4. `-DryRun` 找不到产物时输出 `BLOCKED` 并非零退出。
5. 正常运行时如果 canonical 产物存在，直接执行它；不存在则报错并退出。

已删除行为：

- 不再接受 `-UseLegacyFallback`
- 不再探测 `control-core/build/bin/**/siligen_cli.exe`
- 不再引用外部 `Backend_CPP\src\adapters\cli`

## 5. CLI 二进制或运行入口的真实产物路径

- 真实启动入口：`apps/control-cli/run.ps1`
- 真实构建入口：`apps/control-cli/CMakeLists.txt`
- 真实 target：`siligen_cli`
- 真实产物路径模式：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_cli.exe`
- 本次验证产物：`C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_cli.exe`

如果短期保留旧名称，只允许在 canonical build root 下做 alias；真实实现仍必须继续指向 `siligen_cli.exe`。

## 6. 已更新的脚本、README、验证入口

已更新：

- `apps/control-cli/run.ps1`
- `apps/control-cli/README.md`
- `README.md`
- `packages/test-kit/src/test_kit/workspace_validation.py`
- `tools/scripts/legacy_exit_checks.py`
- `packages/runtime-host/src/bootstrap/InfrastructureBindingsBuilder.cpp`

新增/更新的验证入口：

- `control-cli-dryrun`
- `control-cli-help`
- `control-cli-bootstrap-check`
- `control-cli-recipe-list`
- `control-cli-dxf-plan`

## 7. 删除 fallback 后的验证结果

本次本地验证：

1. `cmake --build C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build --target siligen_cli --config Debug -- /m:1`
   - 成功生成 `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_cli.exe`
2. `apps/control-cli/run.ps1 -DryRun`
   - 解析到 canonical `siligen_cli.exe`
3. `siligen_cli.exe --help`
   - 帮助输出包含连接调试、运动、点胶、DXF、recipe 命令面
4. `siligen_cli.exe bootstrap-check --config config/machine/machine_config.ini`
   - 通过
5. `siligen_cli.exe recipe list --config config/machine/machine_config.ini`
   - 可离线执行
6. `siligen_cli.exe dxf-plan --file examples/dxf/rect_diag.dxf --config config/machine/machine_config.ini`
   - 可离线执行并返回规划结果
7. `python -m test_kit.workspace_validation --profile local --suite apps`
   - `10 passed, 0 failed`

当前本地仍需单独记录的功能性阻塞：

- 命令名：`dxf-augment`
- CLI 入口位置：`apps/control-cli/CommandHandlers.Dxf.cpp`
- 当前实际实现位置：`packages/process-runtime-core/src/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.stub.cpp`
- 阻塞原因：当前 build 以 `SILIGEN_ENABLE_CGAL=OFF` 构建，`ContourAugmenterAdapter` 返回 `NOT_IMPLEMENTED`
- 说明：这不是 legacy fallback；命令已迁入 canonical CLI，但本地构建未启用 CGAL 特性

## 8. 对删除 `control-core/build/bin/**/siligen_cli.exe` 的结论

当前结论：

- 从 CLI 入口、命令实现、默认脚本、验证链路四个维度看，`control-core/build/bin/**/siligen_cli.exe` 已不再是必需项。
- 当前没有 CLI 命令仍阻止删除该 legacy 产物。
- `dxf-augment` 的 CGAL 特性阻塞不再要求保留 legacy CLI；它只影响该命令在当前本地构建中的功能可用性。

剩余需要单独继续跟踪的不是 CLI fallback，而是：

1. `control-core` 仍是共享库图与 `third_party` owner。
2. 其他非 CLI residual consumer 仍需按 `control-core` 总体删除计划继续清理。

## 9. Wave 3C 入口收口补充

当前补充结论：

1. CLI 默认配置路径仍是 `config/machine/machine_config.ini`，但真实解析统一经 `ResolveConfigFilePathWithBridge(...)`，旧 alias 继续 hard-fail。
2. `dxf-preview` 未显式设置 `SILIGEN_DXF_PREVIEW_SCRIPT` 时，默认脚本入口为 `tools/engineering-data-bridge/scripts/generate_preview.py`。
3. `packages/engineering-data/scripts/generate_preview.py` 继续保留为 owner/维护入口，不再被描述为工作区默认入口。
4. `SILIGEN_ENGINEERING_DATA_PYTHON`、`SILIGEN_DXF_PREVIEW_PYTHON` 的优先级与兼容策略不变。


