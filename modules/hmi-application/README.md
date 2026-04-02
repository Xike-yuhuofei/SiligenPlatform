# hmi-application

`modules/hmi-application/` 是 `M11 hmi-application` 的 canonical owner 入口。

## Owner 范围

- HMI 启动恢复、运行态退化、preview 会话与生产前置校验的 owner 语义。
- 人机展示对象、状态聚合与 host-to-owner 边界。
- HMI 域 owner 专属 contracts 与宿主调用边界。

## Owner 入口

- 构建入口：`modules/hmi-application/CMakeLists.txt`（target：`siligen_module_hmi_application`）。
- 模块契约入口：`modules/hmi-application/contracts/README.md`。
- 测试入口：`modules/hmi-application/tests/CMakeLists.txt`。

## 边界约束

- `apps/hmi-app/`、`apps/control-cli/` 仅承担宿主/命令入口，不承载 `M11` 终态 owner 事实。
- `apps/hmi-app/src/hmi_client/ui/main_window.py` 只允许保留 Qt 宿主、signal/slot、消息框、status bar 更新、协议调用和 HTML 渲染。
- 启动恢复、preview 语义校验、preview 重同步与生产前置 block reason 必须落在 `modules/hmi-application/application/hmi_application/`。
- 跨模块稳定应用契约维护在 `shared/contracts/application/`。
- `M11` 仅承载 HMI 业务 owner 语义，不直接越权承担设备实现或运行时执行 owner 职责。

## 当前 owner surface

- `apps/hmi-app/`
- `shared/contracts/application/`
- `modules/hmi-application/contracts/`
- `modules/hmi-application/application/hmi_application/launch_state.py`
- `modules/hmi-application/application/hmi_application/preview_session.py`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- `modules/hmi-application/tests/unit/` 为 `M11` owner 规则 canonical 测试面；`apps/hmi-app/tests/unit/` 只保留 thin-host 集成断言。
- 终态实现必须只落在该模块 canonical 骨架内，不再引入桥接目录或旁路 owner 面。

## 当前测试面

- `tests/unit/`
  - 冻结 launch state projection、startup-state 与 preview protocol consumption
- `tests/contract/`
  - 冻结 `launch_supervision_contract.py` 的快照/事件不变量
