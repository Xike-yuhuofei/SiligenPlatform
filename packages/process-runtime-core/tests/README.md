# process-runtime-core 测试入口

`packages/process-runtime-core/tests` 是核心内核测试的 canonical 物理位置。

当前 target：

- `siligen_unit_tests`
- `siligen_pr1_tests`

当前构建接入方式：

- package 侧定义 target：`packages/process-runtime-core/tests/CMakeLists.txt`
- test target 直接依赖 `siligen_process_runtime_core`
- `control-core/tests/CMakeLists.txt` 仅保留兼容包装并转发到 package 侧

当前源码位置：

- `tests/unit/domain/*`
- `tests/unit/motion/*`
- `tests/unit/recipes/*`
- `tests/unit/system/*`
- `tests/unit/dispensing/*`
- `tests/unit/infrastructure/adapters/planning/dxf/*`

迁移原则：

- 新增核心内核测试直接放在 package 侧
- 测试依赖优先通过 `siligen_process_runtime_core` 暴露的边界接入
- 不再为测试新增 `../../control-core/...` include 或 link
- 只有 DXF 解析相关测试额外补链 `siligen_parsing_adapter`

当前验证命令：

```powershell
cmake --build tmp\process-runtime-core-cutover-build-clean --target siligen_unit_tests --config Debug -j 1 -- /v:m
cmake --build tmp\process-runtime-core-cutover-build-clean --target siligen_pr1_tests --config Debug -j 1 -- /v:m
ctest --test-dir tmp\process-runtime-core-cutover-build-clean -C Debug -R "siligen_(unit|pr1)_tests" --output-on-failure
```

