# NOISSUE Wave 2B-A Root Build/Test/Source-Root Cutover Design

- 状态：Done
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`branchSafe=feat-dispense-NOISSUE-e2e-flow-spec-alignment`，`timestamp=20260322-155701`

## 范围

1. 只分析 root build/test/source-root cutover 的设计边界与切换顺序。
2. 只基于以下正式入口建立当前事实与未来切换合同：
   - `CMakeLists.txt`
   - `tests/CMakeLists.txt`
   - `build.ps1`
   - `test.ps1`
   - `tools/build/build-validation.ps1`
3. 给出受影响入口矩阵、原子变更边界、桥接再翻转策略、最小回滚单元。
4. 给出未来实施时应执行的验证命令与预期结果。

## 非范围

1. 不修改任何构建入口，不执行任何代码实现。
2. 不设计 runtime 默认资产路径切换；`config/`、`data/`、`integration/reports/` 保持现状。
3. 不处理 `control-core` residual dependency、`device-hal`、`third_party` owner 迁移。
4. 不覆盖 `ci.ps1` 或 `.github/workflows/*` 的具体改造方案；这里只定义 root build/test/source-root cutover 的本地合同。
5. 不接受“一次性直接翻转 source root 后再看测试”的实施方式。

## 当前事实

1. `CMakeLists.txt` 以 `CMAKE_SOURCE_DIR` 直接推导 `SILIGEN_WORKSPACE_ROOT`，并把该根作为 canonical C++ superbuild source root。
2. 同一文件随后从 `${SILIGEN_WORKSPACE_ROOT}/cmake/workspace-layout.env` 加载 layout，并把 `packages/shared-kernel`、`packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway` 以及 `apps/` 全部挂到同一根级 superbuild 下。
3. `tests/CMakeLists.txt` 不从 package 自治入口聚合测试，而是从 `tests/..` 回退到仓库根，再经同一份 layout 聚合 `shared-kernel`、`process-runtime-core`、`runtime-host` 的 canonical 测试入口。
4. `build.ps1` 本身只是公开 wrapper，但它把正式 build 入口固定到 `tools/build/build-validation.ps1`。
5. `tools/build/build-validation.ps1` 通过脚本位置反推 `$workspaceRoot`，随后显式设置 `$workspaceSourceRoot = $workspaceRoot`，并以 `cmake -S $workspaceSourceRoot -B $controlAppsBuild` 固化 root superbuild configure 行为。
6. 同一 build 脚本还读取 `CMakeCache.txt` 中的 `CMAKE_HOME_DIRECTORY`；若缓存中的 source root 与当前 `$workspaceSourceRoot` 不一致，则清空 build root。这说明 build 缓存与 source root 已经被视为同一合同。
7. 该 build 脚本最后输出 `control-apps source root: ...`，并明确声明 `workspace root 已成为唯一 C++ superbuild source root`，因此当前 canonical source root 并不是隐含事实，而是显式门禁口径。
8. `test.ps1` 本身不直接执行 `cmake -S`，但它把正式 test 入口固定到 `tools/scripts/invoke-workspace-tests.ps1`，后者又以仓库根为 `workspaceRoot` 编排测试与报告目录。
9. 当前 `packages/test-kit/src/test_kit/workspace_validation.py` 进一步把 `WORKSPACE_ROOT`、`CONTROL_APPS_BUILD_ROOT` 和 `integration/reports` 固定为默认验证环境；其中本地 `packages` suite 直接消费由 root build 产出的 `siligen_shared_kernel_tests.exe`、`siligen_unit_tests.exe`、`siligen_pr1_tests.exe`。
10. 结论：当前 canonical source root 并不是由单一文件独立定义，而是由 root CMake、root tests 聚合、root build orchestration、root test orchestration 共同定义的对外合同。

## 关键结论

### 当前 canonical source root 由哪些入口共同定义

1. 直接定义 source root 的入口是：
   - `CMakeLists.txt`
   - `tests/CMakeLists.txt`
   - `tools/build/build-validation.ps1`
2. 对外暴露并固化该合同的正式入口是：
   - `build.ps1`
   - `test.ps1`
3. 因此当前 canonical source root 的正式定义应视为“5 个入口共同定义的一组合同”，而不是只看 `CMakeLists.txt`。

### 哪些入口必须原子变更

1. 必须原子变更的核心三件套是：
   - `CMakeLists.txt`
   - `tests/CMakeLists.txt`
   - `tools/build/build-validation.ps1`
2. 原因：
   - `CMakeLists.txt` 决定 superbuild 的真实 source root 与子目录图。
   - `tests/CMakeLists.txt` 决定测试聚合图是否与构建图指向同一 canonical package root。
   - `tools/build/build-validation.ps1` 决定公开 build 命令到底把 `cmake -S` 指向哪里，并决定缓存清理逻辑是否识别新 root。
3. `build.ps1` 与 `test.ps1` 如果继续保持纯 wrapper，可以不成为“必须编辑”的文件，但它们必须纳入同一原子发布与同一验证窗口，不能出现 wrapper 仍发布旧合同、内部脚本已切到新合同的分裂态。

### 哪些动作可以先桥接再翻转

1. 可以先桥接的是“source-root 选择机制”，不能直接桥接成“双 source root 同时生效”。
2. 可桥接动作：
   - 把当前隐式的 repo-root 推导改成显式 selector，且 selector 初始值仍指向当前 workspace root。
   - 把 `tests/CMakeLists.txt` 的相对路径聚合改成通过统一 selector 或统一 layout 间接解析。
   - 把 `tools/build/build-validation.ps1` 的 `$workspaceSourceRoot` 改成读取同一 selector，同时保留 `controlAppsBuild`、目标名、报告根不变。
   - 把 test 编排中的 provenance 校验改成读取同一 selector，并在报告中显式打印被选中的 source root。
3. 不可桥接动作：
   - 不允许 root CMake 图和 build 脚本分别指向不同 source root。
   - 不允许 tests 聚合图先翻而 build 图未翻。
   - 不允许通过保留旧 root 与新 root 双 configure 并存来“观察”行为。

### 最小回滚单元是什么

1. 最小安全回滚单元不是单文件，而是“root superbuild source-root 合同单元”。
2. 该单元至少包含：
   - source-root selector 本身
   - `CMakeLists.txt`
   - `tests/CMakeLists.txt`
   - `tools/build/build-validation.ps1`
3. 若 `build.ps1` 或 `test.ps1` 在实施中也变更了 wrapper 合同，则它们必须并入同一回滚单元。
4. 只回滚其中一项会制造以下失配：
   - `cmake -S` 已翻转，但 root CMake 图仍指向旧入口
   - build 图已翻转，但 tests 仍聚合旧测试根
   - 新旧 `CMAKE_HOME_DIRECTORY` 与 build cache 不匹配，导致假成功或伪失败

## 受影响入口矩阵

| 入口 | 当前 root 定义方式 | 当前承担的合同 | 对 cutover 的敏感点 | 原子级别 |
|---|---|---|---|---|
| `CMakeLists.txt` | `CMAKE_SOURCE_DIR -> SILIGEN_WORKSPACE_ROOT` | 定义 canonical C++ superbuild、apps/packages 注册顺序、测试开关 | 任何 source-root 翻转都会改变 configure 根、layout 根、`add_subdirectory` 根 | 必须原子 |
| `tests/CMakeLists.txt` | `CMAKE_CURRENT_SOURCE_DIR/.. -> SILIGEN_WORKSPACE_ROOT` | 定义 root tests 聚合图 | 若继续指向旧 package tests 根，会造成 build/test split-brain | 必须原子 |
| `build.ps1` | 固定委托 `tools/build/build-validation.ps1` | 对外公开 build 命令名与参数面 | 即使不改文件，也必须与内部脚本同波次验证 | 公开合同原子 |
| `test.ps1` | 固定委托 `tools/scripts/invoke-workspace-tests.ps1` | 对外公开 test 命令名、suite/report 参数面 | 即使不改文件，也必须验证其仍指向同一 source-root 合同 | 公开合同原子 |
| `tools/build/build-validation.ps1` | 脚本位置反推 `$workspaceRoot`，再设 `$workspaceSourceRoot = $workspaceRoot` | 决定 `cmake -S`、build root reset、artifact 根与 source-root 日志 | 若只翻这里不翻 root CMake/tests，会直接构成失配 | 必须原子 |

## 切换步骤草案

1. 冻结当前外部合同。
   - `build.ps1`、`test.ps1` 的命令面、suite 名称、`integration/reports` 根、`controlAppsBuild` 产物位置、可执行文件名都先视为不可变。
2. 引入单一的 superbuild source-root selector。
   - selector 必须只有一个来源，且初始值仍指向当前 workspace root。
   - 在 selector 未落地前，不允许开始任何 root source-root 翻转。
3. 先做桥接，不做翻转。
   - `CMakeLists.txt`、`tests/CMakeLists.txt`、`tools/build/build-validation.ps1`、test provenance 校验全部改为读取同一 selector。
   - 这一阶段要求外部命令、target 名称、artifact 输出根、报告根全部不变。
4. 增加 fail-fast 一致性检查。
   - build 脚本必须校验“selector 解析出的 source root”和 `CMAKE_HOME_DIRECTORY` 一致。
   - test 报告必须打印选中的 source root，防止 build/test 指向不同根而无人察觉。
5. 在桥接态完成全量基线回归。
   - 只有桥接态在 `apps/packages/integration/protocol-compatibility/simulation` 全部通过，才允许进入 flip。
6. 执行单点翻转。
   - 只翻 selector 的值与与之耦合的 root graph 解析逻辑。
   - 同一提交窗口内同步生效 `CMakeLists.txt`、`tests/CMakeLists.txt`、`tools/build/build-validation.ps1`。
7. 翻转后立即跑根级验证，不允许“先翻再补测”。
   - 若任一根级 build/test gate 失败，直接进入回滚单元。
8. 稳定后再考虑删除桥接层。
   - 删除桥接必须是 cutover 通过后的独立后续波次，不能与首次 flip 混做。

## 回滚步骤草案

1. 以“root superbuild source-root 合同单元”为单位回滚。
   - 回滚 selector。
   - 回滚 `CMakeLists.txt`、`tests/CMakeLists.txt`、`tools/build/build-validation.ps1` 的同波次改动。
   - 若 wrapper 合同有改动，一并回滚 `build.ps1`、`test.ps1`。
2. 清理或重建 control-apps build root。
   - 必须让 `CMAKE_HOME_DIRECTORY` 回到回滚后的 source root；不能复用混合缓存。
3. 重新执行 root build 基线。
   - 先确认 `control-apps source root`、`CMAKE_HOME_DIRECTORY`、artifact 根全部恢复旧合同。
4. 再执行 root test 基线。
   - 确认 `workspace-validation` 报告回到旧合同，且报告根仍写回同一 `integration/reports` 分支。
5. 回滚完成判定。
   - 只有当 build、test、report root、artifact root 全部回到旧合同时，回滚才算完成。

## 验证命令与预期结果

| 命令 | 用途 | 预期结果 |
|---|---|---|
| `.\build.ps1 -Profile CI -Suite apps` | 验证 root build 公开入口仍有效 | 成功结束；输出中明确打印唯一 `control-apps source root`；`siligen_control_runtime.exe`、`siligen_tcp_server.exe`、`siligen_cli.exe` 仍落到同一 `controlAppsBuild` 根 |
| `.\build.ps1 -Profile Local -Suite packages` | 验证 packages 本地测试产物仍由 root build 暴露 | 成功结束；`siligen_shared_kernel_tests.exe`、`siligen_unit_tests.exe`、`siligen_pr1_tests.exe` 仍在同一 `controlAppsBuild` 根 |
| `.\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave2bA\apps -FailOnKnownFailure` | 验证 apps suite 仍消费 canonical root build 产物 | 成功结束；报告文件仍写到 `integration/reports/verify/wave2bA/apps`；无 missing canonical executable 错误 |
| `.\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave2bA\packages -FailOnKnownFailure` | 验证 packages suite 根级门禁 | 成功结束；`workspace-layout-boundary` 与相关 packages 校验通过；无 `failed`、无 `known_failure` |
| `Get-Content <controlAppsBuild>\CMakeCache.txt | Select-String '^CMAKE_HOME_DIRECTORY:'` | 验证 build cache provenance | 命中值必须与本次 selector 解析出的唯一 source root 完全一致；不允许旧 root / 新 root 混合 |
| `.\test.ps1 -Profile CI -Suite integration,protocol-compatibility,simulation -ReportDir integration\reports\verify\wave2bA\extended -FailOnKnownFailure` | 验证 cutover 未破坏跨层回归 | 成功结束；报告仍集中写入同一 `integration/reports` 根；不允许通过拆分报告根来规避失败 |

## 风险

1. `CMakeLists.txt` 与 `tools/build/build-validation.ps1` 现在分别通过 `CMAKE_SOURCE_DIR` 和脚本位置推导 root；若只改一侧，最容易出现 configure 根与 build 根不一致。
2. `tests/CMakeLists.txt` 当前仍是 root 聚合测试；如果 build graph 已翻而 tests graph 未翻，会出现“应用能编译、测试仍跑旧入口”的伪稳定状态。
3. `test.ps1` 的真实行为依赖 `invoke-workspace-tests.ps1` 和 `workspace_validation.py`；如果 cutover 只盯 5 个入口、不核对测试 provenance，容易漏掉 artifact 路径失配。
4. `build-validation.ps1` 当前的缓存重置逻辑依赖 `CMAKE_HOME_DIRECTORY`；若新方案没有把 selector 与 cache provenance 绑定，会出现旧缓存污染。
5. 现有工作树中 `CMakeLists.txt`、`tests/CMakeLists.txt`、`tools/build/build-validation.ps1` 已存在未提交修改；未来实施必须避免覆盖并发改动。
6. runtime 默认资产路径和 residual dependency 虽不在本设计范围内，但新 source root 若无法继续暴露等价 include、config、data 关系，仍可能间接引爆 build/test 回归。

## 阻断项

1. 目标 superbuild source root 的最终落点尚未冻结；在目标目录未冻结前，只能完成桥接设计，不能开始 flip 实施。
2. 当前仓内还没有单一、显式、可审计的 root source-root selector；`repo root` 仍由多个入口各自推导，这是实施 cutover 的首要阻断项。
3. `test.ps1` 所依赖的测试 provenance 仍散落在 `invoke-workspace-tests.ps1` 与 `workspace_validation.py`；若不先把 provenance 打平到同一 selector，无法证明 build/test 同步切换。
4. `controlAppsBuild` 产物根、target 名称和 `integration/reports` 根必须被视为冻结合同；若这些合同在 source-root cutover 同波次被连带改动，回滚单元会失控。
