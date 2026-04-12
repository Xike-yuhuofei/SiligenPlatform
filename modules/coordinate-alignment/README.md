# coordinate-alignment

`modules/coordinate-alignment/` 是 `M5 coordinate-alignment` 的 canonical owner 入口。

## Owner 范围

- 规划链中的坐标系基准对齐语义
- 坐标变换与对齐参数的模块 owner 边界
- 面向 `M6 process-path` 消费的对齐结果口径

## 边界约束

- `workflow`、`process-planning` 仅提供上游输入，不承载 `M5` 终态 owner 事实。
- 运行时入口与执行链不得回写 `M5` owner 事实。
- 跨模块稳定工程契约应放在 `shared/contracts/engineering/`，本模块仅承载 `M5` 专属契约。

## Owner 入口

- 构建入口：`CMakeLists.txt`（目标：`siligen_module_coordinate_alignment`）。
- 契约入口：`contracts/README.md`。
- 测试入口：`tests/CMakeLists.txt`。

## 当前事实来源

- `modules/coordinate-alignment/contracts/include/coordinate_alignment/contracts/CoordinateTransformSet.h`
- `modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h`
- `modules/runtime-execution/adapters/device/include/siligen/device/adapters/hardware/HardwareTestAdapter.h`

## 当前 root surface

- 模块根 target `siligen_module_coordinate_alignment` 当前只暴露 `contracts/`。
- `CoordinateTransformSet` 是当前唯一冻结的 `M5` owner seam；`process-path` 当前也只消费该 seam。
- `contracts/include/coordinate_alignment/contracts/IHardwareTestPort.h` 仍作为 runtime-execution 的 consumer residual public header 留在 root surface；因此 `contracts/` 目前不是纯粹的 `M5` owner-only contract 面。
- `application/`、`adapters/`、`services/`、`examples/` 当前都属于 docs-only shell，不参与 live build。

## Residual Exit

- `domain/machine/` 的 legacy machine/calibration/runtime residual 已退出 live build，本轮后仅保留退出说明文档。
- `domain/machine/ports/IHardwareTestPort.h` dead forwarder 已删除，不再伪装为 live owner root。
- `CalibrationProcess`、`IHardwareConnectionPort` 与 workflow bridge headers 不在当前模块 live roots 中；若继续清理 `contracts/.../IHardwareTestPort.h`，会触发 runtime-execution consumer 迁移，超出本轮低成本边界。
- 本目录禁止重新引入 façade / provider / bridge / compat 壳层来回填已关闭的 residual。

## 当前测试面

- `tests/contract/`
  - 冻结 `CoordinateTransformSet` 默认公开面与基线变换样本
- `tests/golden/`
  - 冻结包含 `origin-offset`、`rotation-z` 的对齐基线样本
- `CalibrationProcess` residual family 已退出 live build，不再作为 `M5` owner 证明。
- `tests/unit/`、`tests/integration/`、`tests/regression/` 当前仅保留 skeleton 占位目录，不参与 live 测试收敛。
