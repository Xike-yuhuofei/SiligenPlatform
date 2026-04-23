# Shared-Kernel Cutover

更新时间：`2026-03-19`

## 1. 结论

- `packages/shared-kernel` 已成为 shared-kernel 的真实源码 owner。
- `control-core/modules/shared-kernel` 已不再承担 live source root、live include root 或 live build owner。
- `control-core/modules/shared-kernel` 已于 `2026-03-19` 完成物理删除；当前不再存在目录级阻塞，只保留删除后门禁与验收记录。

边界保持：

- `shared-kernel` 只承接稳定公共基础能力。
- 不承接设备驱动、运行时装配、recipe 持久化、日志实现或其他业务倾向代码。

## 2. 本次实际切换

### 2.1 真实源码 owner

`packages/shared-kernel` 当前真实承接的稳定公共能力包括：

- `include/siligen/shared/basic_types.h`
- `include/siligen/shared/axis_types.h`
- `include/siligen/shared/numeric_types.h`
- `include/siligen/shared/point2d.h`
- `include/siligen/shared/error.h`
- `include/siligen/shared/result.h`
- `include/siligen/shared/result_types.h`
- `include/siligen/shared/strings/string_manipulator.h`
- `src/strings/string_manipulator.cpp`

其中两处关键切换已经完成：

- `packages/shared-kernel/include/siligen/shared/point2d.h`
  - 已移除 `boost::geometry` 和 `BOOST_GEOMETRY_REGISTER_POINT_2D`
  - 距离计算改为标准库 `std::sqrt`
- `packages/shared-kernel/src/strings/string_manipulator.cpp`
  - 已移除 `boost/algorithm/string.hpp`
  - `Trim`、`ToLower`、`ToUpper`、`Replace`、`ReplaceAll`、`Join` 已改为标准库实现

这意味着 `packages/shared-kernel` 不再通过 Boost 间接依赖 `control-core/third_party`。

### 2.2 CMake / include / link 切换

- `packages/shared-kernel/CMakeLists.txt`
  - 只公开 `include/`
  - 不再回落到 `control-core/third_party`
  - 不再把 `control-core` 视为 source root
- `siligen_shared_kernel`
  - 已能由 canonical package 目录独立构建
  - control-core 构建图中实际编译的也是 canonical target，而不是 `modules/shared-kernel`
- `tools/scripts/legacy_exit_checks.py`
  - 已移除 `packages/shared-kernel/CMakeLists.txt` 的 fallback allowlist
  - `2026-03-19` 本地执行结果为 `failed_rules=0`、`findings=0`

### 2.3 测试切换

已验证的测试面：

- `siligen_shared_kernel`
- `siligen_shared_kernel_tests`

当前测试结果：

- `ctest --test-dir build/shared-device-cutover --output-on-failure -R siligen_shared_kernel_tests -C Debug`
- 结果：`1/1 passed`

## 3. 已完成切换的依赖面

### 3.1 已完成

- shared-kernel 真实源码 owner：`packages/shared-kernel`
- shared-kernel 公开 include root：`packages/shared-kernel/include`
- shared-kernel 标准库替换：
  - `Point2D` 不再依赖 Boost Geometry
  - `StringManipulator` 不再依赖 Boost Algorithm
- shared-kernel 构建：
  - control-core 图内目标构建通过
  - package standalone 构建通过

### 3.2 当前仓内扫描结论

- 未发现 live code/include/link 继续把 `control-core/modules/shared-kernel` 当作 shared-kernel 的真实 owner。
- `control-core/modules/CMakeLists.txt` 也不再把 `shared-kernel` 当作默认可回落子目录。

## 4. 物理删除与回滚点

已执行：

1. 删除前备份：
   - `D:\Projects\SiligenSuite\tmp\legacy-removal-backups\20260319-160038\control-core\modules\shared-kernel`
2. 物理删除：
   - `control-core/modules/shared-kernel/`
3. 删除后门禁：
   - `legacy-exit-check` 新增“目录必须不存在 + direct-path 不得回流”

结论：

- `control-core/modules/shared-kernel` 已完成物理删除。
- 当前若出现回归，可从上述备份根恢复目录并重跑同一套验证。

## 5. 风险与验证

### 5.1 风险

1. shared-kernel 删除本身已通过 workspace 级 build/test/ci 与 legacy-exit 验证，但 `control-core` 整目录仍有 `src/shared`、`src/infrastructure`、`modules/device-hal`、`third_party` 等未迁出职责。
2. `device-shared-boundary` 仍保留既有失败，这不是 shared-kernel 删除引入的回归，而是 `device-hal` / process-runtime seam 尚未收口。

### 5.2 已完成验证

通过：

- `cmake -S control-core -B build/shared-device-cutover -G Ninja -DSILIGEN_BUILD_TESTS=ON -DSILIGEN_FETCH_GTEST=ON -DSILIGEN_USE_PCH=OFF -DSILIGEN_PARALLEL_COMPILE=OFF`
- `cmake --build build/shared-device-cutover --target siligen_shared_kernel siligen_shared_kernel_tests siligen_device_adapters --parallel 8`
- `ctest --test-dir build/shared-device-cutover --output-on-failure -R siligen_shared_kernel_tests -C Debug`
- `cmake -S packages/shared-kernel -B build/shared-kernel-standalone -G Ninja`
- `cmake --build build/shared-kernel-standalone --target siligen_shared_kernel --parallel 8`
- `powershell -ExecutionPolicy Bypass -File .\legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\legacy-exit\shared-device-cutover`

结果：

- shared-kernel 已真实脱离 `control-core/modules/shared-kernel`
- shared-kernel 已真实脱离 `control-core/third_party`
- legacy exit checks 通过，未发现回流
- 删除后根级 `build.ps1 -Profile Local -Suite all` 通过
- 删除后 `legacy-exit-check.ps1 -Profile Local -ReportDir integration\reports\verify\control-core-core-subtree-delete-20260319-160038\legacy-exit` 为 `17/17 PASS`
- 删除后 `ci.ps1 -Suite all` 中 `integration/reports/ci/legacy-exit/legacy-exit-checks.md` 仍为 `17/17 PASS`
