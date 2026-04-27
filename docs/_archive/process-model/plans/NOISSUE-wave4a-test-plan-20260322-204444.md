# NOISSUE Wave 4A Test Plan - DXF Legacy Shell Exit

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-204444`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave4a-scope-20260322-204444.md`
- 关联 arch：`docs/process-model/plans/NOISSUE-wave4a-arch-plan-20260322-204444.md`

## 1. 验证原则

1. control-apps build root 上的构建与测试串行执行。
2. 先跑 Python 级兼容/退出 gate，再跑 `packages` 构建与根级 suites，最后跑 `legacy-exit` 归零报告。
3. 对 legacy 兼容面的签收口径从“可转发”改为“必须不存在”。

## 2. Python / compatibility 验证

```powershell
python packages/engineering-data/tests/test_engineering_data_compatibility.py
python packages/engineering-data/tests/test_dxf_pipeline_legacy_exit.py
python integration/scenarios/run_engineering_regression.py
```

预期：

1. canonical API / artifact 语义不变。
2. `dxf_pipeline.*`、`proto.dxf_primitives_pb2` import 必须失败。
3. `pyproject.toml` 不再暴露 legacy CLI alias。

## 3. 构建与根级回归

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite packages
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave4a-closeout\packages -FailOnKnownFailure
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave4a-closeout\protocol -FailOnKnownFailure
```

预期：

1. `process-runtime-core` 与 `engineering-data` 相关 packages 构建通过。
2. `UploadFileUseCaseTest` 继续通过，且外部 launcher 断言 canonical 子命令。
3. `protocol-compatibility` 不出现新的 failed/known_failure。

## 4. Legacy Exit 归零验证

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\wave4a-closeout\legacy-exit
```

预期：

1. `packages/engineering-data/src/dxf_pipeline` 与 `src/proto/dxf_primitives_pb2.py` 缺失规则通过。
2. 不再出现 `dxf_pipeline` import、legacy CLI alias、`proto.dxf_primitives_pb2` 的 live 引用。
3. 活动文档中的 default entry 说明不再写回 legacy 名称。
