# Transport Alias Consumer Audit

更新时间：`2026-03-19`

## 1. 结论

- `siligen_control_gateway_tcp_adapter`
- `siligen_tcp_adapter`
- `siligen_control_gateway`

以上 3 个 transport alias 在本次审计范围内已无 live consumer。

基于该结果，`packages/transport-gateway/CMakeLists.txt` 已于 `2026-03-19` 删除这 3 个 alias 导出；CI/兼容规则已改为验证“canonical target 仍存在，legacy alias 不得回流”。

## 2. 审计范围与方法

审计时间：`2026-03-19`

审计范围：

1. 工作区内活跃代码与脚本：`apps/`、`packages/`、`integration/`、`tools/`、`.github/`、顶层 `hmi-client/`、`control-core/`
2. 排除目录：`.git/`、`build/`、`tmp/`、`reports/`、`node_modules/`、`__pycache__/`
3. 工作区外可见 sibling / 部署目录：`D:\Projects\Backend_CPP`、`D:\Projects\Document`、`D:\Projects\DXF`、`D:\Projects\ERP`、`D:\Deploy`

审计方法：

- PowerShell `Get-ChildItem ... | Select-String` 全量搜索 alias 名称
- CMake 命中再人工区分“定义点 / dead shell / 测试断言 / 真正消费者”
- 对工作区外命中逐条复核其用途是否属于 runtime / build / HIL / deployment 消费者

## 3. 证据摘要

### 3.1 工作区内活跃命中

审计范围内，3 个 alias 名称只命中以下 4 类文件：

1. `packages/transport-gateway/CMakeLists.txt`
   - 删除前的导出定义点，不是消费者
2. `control-core/src/adapters/tcp/CMakeLists.txt`
   - dead shell 定义点；`control-core/src/CMakeLists.txt` 已不再 `add_subdirectory(adapters/tcp)`
3. `control-core/modules/control-gateway/CMakeLists.txt`
   - dead shell 定义点；`control-core/modules/CMakeLists.txt` 已不再 `add_subdirectory(control-gateway)`
4. `packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
   - 兼容规则；本轮已从“验证 alias 存在”改为“验证 alias 不得回流”

未发现：

- `target_link_libraries(...)`、`find_package(...)`、脚本参数、CI 配置中的 live consumer
- `apps/`、`packages/`、`integration/`、`tools/` 下对 alias 的运行态或构建态依赖

### 3.2 工作区外命中

在 `D:\Projects` sibling 项目和 `D:\Deploy` 中，未发现以下任一 alias 名称：

- `siligen_control_gateway_tcp_adapter`
- `siligen_tcp_adapter`
- `siligen_control_gateway`

因此，本轮审计范围内不存在工作区外 alias residual consumer。

## 4. Alias 逐项结论

| alias | canonical target | 审计结果 | 迁移状态 | 删除条件 | 当前状态 |
|---|---|---|---|---|---|
| `siligen_control_gateway_tcp_adapter` | `siligen_transport_gateway` | 未发现 live consumer | 已归零 | 工作区内外无 live consumer，compatibility test 改为防回流 | `2026-03-19` 已删除 |
| `siligen_tcp_adapter` | `siligen_transport_gateway` | 未发现 live consumer | 已归零 | 工作区内外无 live consumer，compatibility test 改为防回流 | `2026-03-19` 已删除 |
| `siligen_control_gateway` | `siligen_transport_gateway_protocol` | 未发现 live consumer | 已归零 | 工作区内外无 live consumer，compatibility test 改为防回流 | `2026-03-19` 已删除 |

## 5. 删除后的自动化约束

- `packages/transport-gateway/tests/test_transport_gateway_compatibility.py`
  - 必须仍然导出 `siligen_transport_gateway`
  - 必须仍然导出 `siligen_transport_gateway_protocol`
  - 不得重新出现 `siligen_control_gateway_tcp_adapter`、`siligen_tcp_adapter`、`siligen_control_gateway`
- `control-core/src/CMakeLists.txt` 不得重新注册 `adapters/tcp`
- `control-core/modules/CMakeLists.txt` 不得重新注册 `control-gateway`

## 6. 风险边界

- 本次工作区外审计范围只覆盖当前机器可见的 `D:\Projects\*` sibling 项目与 `D:\Deploy`
- 若现场另有私有 repo、离线包或未纳入当前机器的运维脚本，删除前仍应在对应目录重跑同一组字符串审计
