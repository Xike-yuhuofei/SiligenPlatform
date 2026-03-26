# NOISSUE Wave 4A Architecture Plan - DXF Legacy Shell Exit

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-204444`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave4a-scope-20260322-204444.md`

## 1. 目标结构

```text
packages/process-runtime-core
  -> DxfPbPreparationService
       -> default: tools/engineering-data-bridge/scripts/dxf_to_pb.py
       -> external launcher: engineering-data-dxf-to-pb

packages/engineering-data
  -> engineering_data.* canonical API
  -> engineering_data.proto.dxf_primitives_pb2
  -> canonical CLI only: engineering-data-*
  -> no dxf_pipeline.* / no proto.dxf_primitives_pb2 / no legacy CLI alias

legacy-exit
  -> missing path gate
  -> text scan gate
  -> current-fact docs
```

## 2. 组件职责

### 2.1 `packages/process-runtime-core`

1. 保持 `.pb` 预处理 override 入口不变：`SILIGEN_DXF_PB_COMMAND`、`SILIGEN_DXF_PROJECT_ROOT`、`SILIGEN_ENGINEERING_DATA_PYTHON`。
2. 外部 DXF 仓库模式仍通过 `scripts/dxf_core_launcher.py` 交互，但子命令必须使用 canonical `engineering-data-dxf-to-pb`。
3. 不再在默认实现或测试中保留 legacy CLI 命令名。

### 2.2 `packages/engineering-data`

1. 只保留 canonical API / CLI / proto owner。
2. 删除 `dxf_pipeline.*` import shim 与 `proto.dxf_primitives_pb2` 兼容层。
3. 将兼容测试由“legacy shell 仍可转发”改为“legacy shell 必须不可用”。

### 2.3 `legacy-exit` / 文档层

1. `dxf-pipeline` 从“冻结防回流”升级到“兼容壳归零门禁”。
2. architecture / README / compatibility 文档只描述 canonical 入口与人工迁移要求。
3. 历史 closeout 保持归档，不回写；本阶段只补新增工件。

## 3. 失败模式与补救

1. 失败模式：外部 DXF launcher 仍断言旧命令名
   - 补救：先更新 `UploadFileUseCaseTest` 和 `DxfPbPreparationService`，再跑 packages suite。
2. 失败模式：compat shell 删除后 protocol/integration 回归失败
   - 补救：说明仍有仓内真实 consumer，被视为 Wave 4A 阻断，不能用兼容壳恢复掩盖问题。
3. 失败模式：legacy 名称从文档或 pyproject 重新写回
   - 补救：升级 `legacy-exit` 缺失路径和文本门禁，并运行报告归零。

## 4. 回滚边界

1. launcher 合同回滚单元：
   - `packages/process-runtime-core/src/application/services/dxf/DxfPbPreparationService.cpp`
   - `packages/process-runtime-core/tests/unit/dispensing/UploadFileUseCaseTest.cpp`
2. compat shell 回滚单元：
   - `packages/engineering-data/pyproject.toml`
   - `packages/engineering-data/src/dxf_pipeline/**`
   - `packages/engineering-data/src/proto/dxf_primitives_pb2.py`
3. current-fact / 门禁回滚单元：
   - `tools/scripts/legacy_exit_checks.py`
   - `docs/architecture/*`
