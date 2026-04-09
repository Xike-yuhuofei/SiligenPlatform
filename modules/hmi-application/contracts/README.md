# hmi-application contracts

`modules/hmi-application/contracts/` 当前不是 live build root。
本目录暂时只保留 contract relocation 目标说明，不再伪装为已承载 live Python contract 的 canonical truth。

## 契约范围

- 下一阶段要承接的模块 owner 专属契约物理落点。
- 用于对照当前已经落在包内 `application/hmi_application/contracts/` 的 live contract。

## 边界约束

- 当前 live 启动监督态 contract 已在 `modules/hmi-application/application/hmi_application/contracts/launch_supervision_contract.py`。
- 跨模块稳定应用契约应维护在 `shared/contracts/application/`。
- 在 contract relocation batch 完成前，本目录不得再被 README / CMake 写成 live public surface。

## 当前对照面

- `shared/contracts/application/commands/`
- `shared/contracts/application/queries/`
- `shared/contracts/application/models/`
- `modules/hmi-application/application/hmi_application/contracts/launch_supervision_contract.py`
