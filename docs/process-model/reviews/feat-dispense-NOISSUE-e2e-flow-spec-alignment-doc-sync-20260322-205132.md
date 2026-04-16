# Doc Sync Review

- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 时间戳：20260322-205132
- 范围：Wave 4A DXF legacy shell exit
- 上游收口：`docs/process-model/plans/NOISSUE-wave3c-closeout-20260322-202853.md`

## 1. 本次同步的文件

1. `packages/engineering-data/README.md`
   - 把兼容窗口描述改成“已结束”，只保留 canonical CLI / import / proto 入口。
2. `packages/engineering-data/docs/compatibility.md`
   - 删除“继续保留兼容壳”的口径，改为迁移要求和 canonical 入口说明。
3. `docs/architecture/dxf-pipeline-strangler-progress.md`
   - 从“仍在 strangler”更新为“compat shell 已退出”。
4. `docs/architecture/dxf-pipeline-final-cutover.md`
   - 更新到兼容壳删除后的 current-fact。
5. `docs/architecture/governance/migration/legacy-exit-checks.md`
   - 把 `dxf-pipeline` 从冻结防回流升级为兼容壳归零门禁。
6. `docs/architecture/removed-legacy-items-final.md`
7. `docs/architecture/redundancy-candidate-audit.md`
8. `packages/process-runtime-core/src/application/usecases/dispensing/README.md`

## 2. 代码与测试写集

1. `packages/process-runtime-core/src/application/services/dxf/DxfPbPreparationService.cpp`
2. `packages/process-runtime-core/tests/unit/dispensing/UploadFileUseCaseTest.cpp`
3. `packages/engineering-data/pyproject.toml`
4. 删除：
   - `packages/engineering-data/src/dxf_pipeline/**`
   - `packages/engineering-data/src/proto/dxf_primitives_pb2.py`
   - `packages/engineering-data/tests/test_dxf_pipeline_legacy_shims.py`
5. 新增：
   - `packages/engineering-data/tests/test_dxf_pipeline_legacy_exit.py`
6. `tools/scripts/legacy_exit_checks.py`

## 3. 新增治理工件

1. `docs/process-model/plans/NOISSUE-wave4a-scope-20260322-204444.md`
2. `docs/process-model/plans/NOISSUE-wave4a-arch-plan-20260322-204444.md`
3. `docs/process-model/plans/NOISSUE-wave4a-test-plan-20260322-204444.md`
4. `docs/process-model/plans/NOISSUE-wave4a-closeout-20260322-205132.md`

## 4. 本次同步的原因

1. 代码已经删除 compat shell；如果文档仍写“继续保留 legacy import/CLI”，会形成明显冲突。
2. `legacy-exit` 已升级到归零门禁，architecture 文档必须与门禁一致。
3. `process-runtime-core` 的外部 launcher 协议已切到 canonical 子命令，README 需要同步这一事实。

## 5. 刻意未改的内容

1. 未回写历史 `docs/process-model/plans/**` 工件。
2. 未把仓外环境迁移状态写成“已完成”，因为这不在仓内证据范围内。
3. 未更改 `tools/engineering-data-bridge/scripts/*` 与 owner 脚本分层设计。

## 6. 结论

1. 当前事实文档与实现已对齐到同一结论：
   - compat shell 已退出
   - canonical `engineering-data-*` 是唯一 CLI 安装面
   - `engineering_data.*` / `engineering_data.proto.dxf_primitives_pb2` 是唯一 Python 入口
2. 本轮文档同步足以支撑 `Wave 4A closeout` 和后续仓外观察期规划。
