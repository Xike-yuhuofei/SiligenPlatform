# Examples

`examples/` 当前不包含任何已启用的示例目标。

原因：

- 顶层 [`examples/CMakeLists.txt`](./CMakeLists.txt) 仅保留占位说明。
- 旧的 Bug 检测示例和限位测试示例已经从当前构建流程中移除。

## 当前状态

- 不会生成 `build/bin/*example*.exe`
- 不存在 `build_examples` 或 `run_all_examples` 这类 CMake 目标
- 如需验证当前代码，请使用顶层构建脚本和 `tests/`

## 如果要新增示例

1. 在本目录添加示例源码。
2. 更新 [`examples/CMakeLists.txt`](./CMakeLists.txt) 明确声明目标。
3. 在顶层 [`README.md`](../README.md) 或 [`docs/README.md`](../docs/README.md) 补充入口说明。
