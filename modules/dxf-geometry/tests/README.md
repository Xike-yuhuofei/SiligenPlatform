# tests

dxf-geometry 的模块级验证入口应收敛到此目录。

## 当前最小正式矩阵

- `unit/services/dxf/DxfPbPreparationServiceTest.cpp`
  - 冻结 PB 生成服务的缓存命中、外部根解析、命令覆盖与失败处理。
- `contract/DxfPbPreparationServiceContractsTest.cpp`
  - 冻结 `DxfPbPreparationService` 公开构造面与 `EnsurePbReady` 的最小稳定行为。
- `golden/DxfPbPreparationServiceCommandGoldenTest.cpp`
  - 冻结非默认 `DxfPreprocessConfig` 组装出的参数快照，防止命令旗标顺序和默认值漂移。
- `integration/DxfPbPreparationServiceIntegrationTest.cpp`
  - 覆盖工作区 `scripts/engineering-data/dxf_to_pb.py` 解析与配置旗标透传闭环。

## 边界

- 模块内 `tests/` 负责 `dxf-geometry` owner 级证明，不用仓库级 `tests/integration` 替代。
- `golden` 只冻结脱敏参数快照，不把临时目录路径写成权威真值。
