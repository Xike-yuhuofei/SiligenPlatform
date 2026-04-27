# NOISSUE Wave 3B Test Plan - Gateway Config Boundary + Preview Consumer Cleanup

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-200303`
- 关联 scope：`docs/process-model/plans/NOISSUE-wave3b-scope-20260322-200303.md`
- 关联 arch：`docs/process-model/plans/NOISSUE-wave3b-arch-plan-20260322-200303.md`

## 1. 验证原则

1. 构建与根级 suite 串行执行，不在同一 build root 上并行。
2. 先跑静态/单元 gate，再跑 app build，最后跑根级 suites。
3. 只签收 `Wave 3B` 的两条边界收口，不扩展到 `control-cli` 其余路径治理。

## 2. 静态与单元验证

### 2.1 gateway 合同收窄

命令：

```powershell
python packages/transport-gateway/tests/test_transport_gateway_compatibility.py
```

预期：

1. `TcpServerHostOptions` 不再暴露 `config_path`
2. `TcpCommandDispatcher` 不再接收或保存 `config_path`
3. `control-tcp-server` 仍只通过公开 builder/host wiring

### 2.2 preview artifact canonical consumer

命令：

```powershell
python packages/engineering-contracts/tests/test_engineering_contracts.py
```

预期：

1. preview fixture 与 `PreviewArtifact` consumer 兼容
2. 不再依赖 `apps/hmi-app` 的本地 preview dataclass

### 2.3 HMI 单测

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File apps/hmi-app/scripts/test.ps1
```

预期：

1. gateway launch 单测继续通过
2. preview gate / protocol preview 合同继续通过
3. 删除 `test_preview_client.py` 后无 import 断裂

## 3. 构建与根级回归

### 3.1 app build

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Profile CI -Suite apps
```

预期：

1. `siligen_tcp_server` 成功构建
2. HMI app suite 依赖不因 preview residual 删除而断裂

### 3.2 根级 suites

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\wave3b-closeout\packages -FailOnKnownFailure
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\wave3b-closeout\apps -FailOnKnownFailure
powershell -NoProfile -ExecutionPolicy Bypass -File .\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\wave3b-closeout\protocol -FailOnKnownFailure
```

预期：

1. `packages`、`apps`、`protocol-compatibility` 全绿
2. 不出现新的 `failed` 或 `known_failure`

## 4. 负向搜索

1. `rg -n "config_path_|std::string config_path" packages/transport-gateway apps/control-tcp-server`
2. `rg -n "DxfPipelinePreviewClient|DxfPreviewResult|integrations/dxf_pipeline/preview_client" apps packages/engineering-contracts docs/architecture`

预期：

1. 活跃源码中 `config_path` gateway 合同面清零
2. 活跃源码与当前事实文档中 `preview_client` 口径清零
