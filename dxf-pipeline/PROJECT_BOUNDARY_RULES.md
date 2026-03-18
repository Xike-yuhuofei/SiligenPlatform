# 项目边界规则

## 项目职责
- 承载 DXF 几何解析、路径提取、排序优化、轨迹采样与 proto/json 导出。
- 为 `control-core` 提供稳定的离线预处理结果，而不是直接参与实时控制。

## 允许依赖
- 可以依赖本项目内部 geometry、path、trajectory、contracts、io、cli 与 adapters 模块。
- 可以维护 `proto` 与生成代码，但必须保持版本化管理。

## 禁止事项
- 禁止直接依赖 `control-core` 的内部 domain/application/infrastructure 实现。
- 禁止承担 HMI UI 逻辑或 DXF 编辑器交互逻辑。
- 禁止把实时硬件控制、急停、安全联锁等职责放入本项目。

## 对外交互规则
- 对 `control-core` 仅输出版本化的 proto/json 契约结果。
- 输入输出格式变更必须同步更新契约文档和兼容性测试。
- 不允许通过复制主控内部类型来实现对接。

## 测试规则
- 单元测试覆盖几何算法、路径处理、轨迹采样和报告生成。
- 集成测试覆盖 proto 兼容性、轨迹生成和控制核心样例回归。
- 性能基准独立维护，不与实时控制测试混用。
