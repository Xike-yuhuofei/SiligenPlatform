# shared-kernel

`shared-kernel` 是 shared kernel 的 canonical owner，承接稳定共享基础代码；`control-core/modules/shared-kernel` 已于 `2026-03-19` 物理删除，不再保留 legacy 实现目录。

当前真实承接内容：

- `numeric_types.h`
- `axis_types.h`
- `basic_types.h`
- `point2d.h`
- `error.h`
- `result.h` / `result_types.h`
- `strings/string_manipulator.*`

规则：

- 只放稳定、跨包复用、无业务倾向的基础能力
- 不吸收设备、流程、运行时宿主或配方仓储实现
- 不反向依赖任何业务包或 `control-core/modules/*`
- 不再依赖 `control-core/third_party` 或任何 package 私有 vendor 目录

当前兼容关系：

- `control-core/CMakeLists.txt` 会先注册本包的 `siligen_shared_kernel`
- `control-core/src/shared` 通过链接 `siligen_shared_kernel` 获取公开头，不再手工 include legacy/canonical 路径
- 已删除的 `control-core/modules/shared-kernel` 目录受 `legacy-exit-check` 保护，不再允许作为默认 target、兼容壳或真实头文件来源回流
- `StringManipulator` 与 `Point2D` 已改为标准库实现，默认构建不再要求 Boost include root

测试：

- `packages/shared-kernel/tests` 承接 canonical 单测
- 工作区真实入口：
  - `powershell -File .\build.ps1 -Profile Local -Suite packages`
  - `powershell -File .\test.ps1 -Profile Local -Suite packages`
