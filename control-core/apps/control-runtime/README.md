# control-runtime

运行时装配与系统宿主目录。

## Canonical 路径

- 根级 canonical app：`D:\Projects\SiligenSuite\apps\control-runtime`

## 当前角色

- 本目录只保留旧 target 名称、少量文档与兼容壳
- 根级 `apps/control-runtime` 才是后续工作默认引用的 canonical 入口
- 若需要说明入口位置、运行方式或后续迁移归宿，应优先更新根级 canonical 目录

本目录已不再承载宿主实现；相关源码已迁移到：

- `D:\Projects\SiligenSuite\packages\runtime-host\src`
- `D:\Projects\SiligenSuite\packages\process-runtime-core\src\application\usecases\system`

兼容说明（非默认入口）：

- `siligen_control_runtime` 旧 target 名称仍保留，但内部直接转发到 `siligen_runtime_host`
- `src/application/container/*`
- `src/infrastructure/adapters/system/*`
- `src/infrastructure/security/*`
- `src/infrastructure/bootstrap/*`
- `src/shared/bootstrap/*`

以上 legacy 路径当前仅保留兼容 include / 兼容编译入口。

补充说明：

- 对整个工作区而言，本目录不再是根级 canonical app，只是 `apps/control-runtime` 的 legacy 实现壳。
- 当前仓内尚未产出独立 `control-runtime` 可执行文件；根级 wrapper 会明确报告这一阻塞，而不是伪造可运行入口。
