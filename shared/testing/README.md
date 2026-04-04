# shared/testing

`shared/testing/` 是仓库级测试支撑的 canonical 子根，统一承载跨模块复用的测试基础设施。

## 归位范围

以下共用能力应在该根统一维护：

- workspace validation 编排与结果模型（`passed`、`failed`、`known_failure`、`skipped`）。
- validation layer / execution lane 模型与路由 helper。
- evidence bundle、case index、failure details 等 shared report helper。
- shared asset catalog、fixture catalog 与 fault scenario helper。
- 跨模块复用的断言、夹具、校验辅助、测试数据构造器。
- 根级 `test.ps1`、`ci.ps1` 与本地 validation gate 共享的测试支撑入口。

## 边界约束

- 仅承载“跨模块复用”的测试支撑，不承载业务 owner 实现。
- 仓库级场景编排与报告产物应保留在 `tests/` 或 `tests/reports/`。
- 禁止在 `shared/testing/` 放置生产运行逻辑、部署流程和单模块私有临时脚本。

## 当前承载面

- `shared/testing/test-kit/pyproject.toml`
- `shared/testing/test-kit/src/test_kit/runner.py`
- `shared/testing/test-kit/src/test_kit/validation_layers.py`
- `shared/testing/test-kit/src/test_kit/asset_catalog.py`
- `shared/testing/test-kit/src/test_kit/evidence_bundle.py`
- `shared/testing/test-kit/src/test_kit/workspace_layout.py`
- `shared/testing/test-kit/src/test_kit/workspace_validation.py`

## Phase 8 Governance Surface

- `asset_catalog.py` 现在同时承载 canonical asset inventory、deprecated root guard、baseline manifest 解析与 duplicate-source 检测。
- `tests/baselines/baseline-manifest.json` 是 baseline provenance 真值，`shared/testing` 负责提供读取与校验 helper，不负责把 report 输出升级为 baseline。
- `data/baselines/**` 已被降级为 deprecated redirect；若重新出现资产文件，workspace gate 会直接阻断。

## 构建与验证入口

- CMake 统一目标：`siligen_shared_testing`（兼容别名：`siligen_test_kit`）。
- workspace validation 正式入口优先使用根级 `.\test.ps1` / `.\ci.ps1`；`python -m test_kit.workspace_validation` 仅在已完成环境引导并注入 `PYTHONPATH=shared\testing\test-kit\src` 时作为底层模块入口。

## 迁移与退出规则

- 新增共用测试支撑优先落在 `shared/testing/`。
- 不允许在 canonical 根之外继续维护第二套 test-kit 事实。
- 兼容 wrapper/别名若仍存在，必须可审计且具备明确退出证据。
