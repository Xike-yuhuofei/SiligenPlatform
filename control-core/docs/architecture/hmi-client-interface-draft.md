# HMI Client 接口草案

## 说明

由于 `src/adapters/hmi` 已按边界拆出，本文件不在 `control-core` 内落 HMI 实现代码，只定义建议由 `hmi-client` 落地的客户端抽象、模块边界与文件布局。

目标：

- 消除页面直接拼 JSON
- 把 HMI 对 TCP 的访问收敛为稳定的 Python client 层
- 让页面层只依赖业务动作，不依赖协议细节

## 推荐目录落点

建议在 `hmi-client` 中采用如下结构：

```text
hmi-client/
├─ src/
│  ├─ app/
│  │  ├─ clients/
│  │  │  ├─ base_client.py
│  │  │  ├─ system_client.py
│  │  │  ├─ motion_client.py
│  │  │  ├─ dispensing_client.py
│  │  │  ├─ recipe_client.py
│  │  │  ├─ dxf_client.py
│  │  │  └─ event_client.py
│  │  ├─ protocol/
│  │  │  ├─ request_envelope.py
│  │  │  ├─ response_envelope.py
│  │  │  ├─ event_envelope.py
│  │  │  └─ error_codes.py
│  │  ├─ transport/
│  │  │  ├─ tcp_transport.py
│  │  │  ├─ backend_manager.py
│  │  │  └─ reconnect_policy.py
│  │  ├─ viewmodels/
│  │  │  ├─ system_view_model.py
│  │  │  ├─ motion_view_model.py
│  │  │  ├─ dispensing_view_model.py
│  │  │  ├─ recipe_view_model.py
│  │  │  └─ dxf_view_model.py
│  │  └─ dto/
│  │     ├─ motion_dto.py
│  │     ├─ recipe_dto.py
│  │     ├─ dispensing_dto.py
│  │     └─ dxf_dto.py
└─ tests/
   ├─ contract/
   ├─ clients/
   └─ viewmodels/
```

## 分层职责

### `transport`

职责：

- 建立 TCP 连接
- 发送原始请求
- 接收原始响应与事件
- 管理自动重连、心跳、后端启动

不负责：

- method 命名
- 业务参数语义
- 页面状态更新

### `protocol`

职责：

- 定义请求/响应/事件的 Python 数据结构
- 负责 JSON 序列化与反序列化
- 统一错误码映射

不负责：

- socket 连接
- 业务动作封装

### `clients`

职责：

- 把页面动作映射为稳定的业务方法
- 负责 method 命名与参数组装
- 把响应解码成 DTO 或领域无关对象

不负责：

- 控件状态管理
- PyQt 控件逻辑

### `viewmodels`

职责：

- 面向页面提供命令式接口
- 管理加载态、错误态、当前数据快照
- 消费 `clients` 返回结果

不负责：

- 拼 JSON
- 直接操作 socket

## 基础抽象

### `BaseClient`

建议职责：

- 统一 `send_request(method, params)`
- 统一超时、日志、异常翻译
- 统一成功/失败响应校验

建议接口：

```python
class BaseClient:
    def __init__(self, transport, error_mapper):
        self._transport = transport
        self._error_mapper = error_mapper

    async def send_request(self, method: str, params: dict | None = None) -> dict:
        ...
```

### `TcpTransport`

建议职责：

- `connect()`
- `disconnect()`
- `send_and_wait()`
- `subscribe()`
- `start_backend_if_needed()`

建议接口：

```python
class TcpTransport:
    async def connect(self) -> None: ...
    async def disconnect(self) -> None: ...
    async def send_and_wait(self, payload: dict, timeout: float = 5.0) -> dict: ...
    def subscribe(self, event_name: str, callback) -> None: ...
```

### `BackendManager`

建议职责：

- 查找 `siligen_tcp_server.exe`
- 启动/停止后端
- 健康检查
- 向 HMI 暴露“后端是否可用”

建议接口：

```python
class BackendManager:
    async def ensure_backend_ready(self) -> None: ...
    async def start_backend(self) -> None: ...
    async def stop_backend(self) -> None: ...
    async def ping(self) -> bool: ...
```

## 各业务 Client 草案

### `SystemClient`

职责：

- 系统初始化
- 紧急停止
- 系统状态查询

建议接口：

```python
class SystemClient(BaseClient):
    async def initialize(self) -> dict: ...
    async def shutdown(self) -> dict: ...
    async def emergency_stop(self, source: str = "hmi") -> dict: ...
    async def get_status(self) -> dict: ...
```

对应 method：

- `system.initialize`
- `system.shutdown`
- `system.emergency_stop`
- `system.status.get`

### `MotionClient`

职责：

- 回零
- Jog
- 点位移动
- 状态与位置查询

建议接口：

```python
class MotionClient(BaseClient):
    async def home(self, axes: list[str], mode: str = "auto") -> dict: ...
    async def start_jog(self, axis: str, direction: int, velocity: float) -> dict: ...
    async def stop_jog(self, axis: str) -> dict: ...
    async def move_to(self, x: float | None = None, y: float | None = None,
                      z: float | None = None, velocity: float | None = None) -> dict: ...
    async def get_status(self, axis: str | None = None) -> dict: ...
    async def get_position(self) -> dict: ...
```

对应 method：

- `motion.home`
- `motion.jog.start`
- `motion.jog.stop`
- `motion.move_to`
- `motion.status.get`
- `motion.position.get`

### `DispensingClient`

职责：

- 启停胶阀
- 暂停/恢复
- 查询点胶状态

建议接口：

```python
class DispensingClient(BaseClient):
    async def start(self, recipe_id: str | None = None, profile_id: str | None = None) -> dict: ...
    async def stop(self) -> dict: ...
    async def pause(self) -> dict: ...
    async def resume(self) -> dict: ...
    async def get_status(self) -> dict: ...
```

对应 method：

- `dispense.start`
- `dispense.stop`
- `dispense.pause`
- `dispense.resume`
- `dispense.status.get`

### `RecipeClient`

职责：

- 配方查询
- 配方创建
- 草稿与发布
- 审计与 bundle

建议接口：

```python
class RecipeClient(BaseClient):
    async def list_recipes(self, filters: dict | None = None) -> dict: ...
    async def get_recipe(self, recipe_id: str) -> dict: ...
    async def create_recipe(self, payload: dict) -> dict: ...
    async def create_draft(self, recipe_id: str, template_id: str, version_label: str) -> dict: ...
    async def update_draft(self, recipe_id: str, version_id: str, params: list[dict]) -> dict: ...
    async def publish(self, recipe_id: str, version_id: str, actor: str) -> dict: ...
    async def list_audit(self, recipe_id: str, version_id: str | None = None) -> dict: ...
    async def export_bundle(self, recipe_id: str, export_file: str) -> dict: ...
    async def import_bundle(self, import_file: str, resolution: str = "rename") -> dict: ...
```

对应 method：

- `recipe.list`
- `recipe.get`
- `recipe.create`
- `recipe.draft.create`
- `recipe.draft.update`
- `recipe.publish`
- `recipe.audit.list`
- `recipe.bundle.export`
- `recipe.bundle.import`

### `DxfClient`

职责：

- 上传 DXF
- 发起规划
- 发起执行
- 查询任务状态

建议接口：

```python
class DxfClient(BaseClient):
    async def upload(self, file_path: str) -> dict: ...
    async def plan(self, upload_id: str, options: dict | None = None) -> dict: ...
    async def execute(self, plan_id: str, options: dict | None = None) -> dict: ...
    async def get_status(self, job_id: str) -> dict: ...
```

对应 method：

- `dxf.upload`
- `dxf.plan`
- `dxf.execute`
- `dxf.status.get`

### `EventClient`

职责：

- 订阅系统事件
- 把原始事件分发给各 ViewModel

建议接口：

```python
class EventClient:
    def subscribe_motion_status(self, callback) -> None: ...
    def subscribe_system_state(self, callback) -> None: ...
    def subscribe_job_progress(self, callback) -> None: ...
    def subscribe_recipe_changed(self, callback) -> None: ...
```

## DTO 草案

建议在 HMI 内只保留“展示友好 DTO”，不要把原始 JSON 深度散落到页面层。

### `motion_dto.py`

建议包含：

- `AxisStatusDto`
- `MachinePositionDto`
- `MotionJobAcceptedDto`

### `recipe_dto.py`

建议包含：

- `RecipeSummaryDto`
- `RecipeDetailDto`
- `RecipeVersionDto`
- `RecipeAuditEntryDto`

### `dispensing_dto.py`

建议包含：

- `DispensingStatusDto`
- `DispensingProfileDto`

### `dxf_dto.py`

建议包含：

- `DxfUploadResultDto`
- `DxfPlanResultDto`
- `DxfExecutionStatusDto`

## ViewModel 草案

ViewModel 面向 PyQt 页面，负责状态，不负责协议。

### `MotionViewModel`

建议公开：

- `initialize()`
- `home_selected_axes()`
- `start_jog(axis, direction)`
- `stop_jog(axis)`
- `refresh_status()`

状态字段建议：

- `is_busy`
- `last_error`
- `axis_status_map`
- `current_position`

### `RecipeViewModel`

建议公开：

- `load_recipe_list()`
- `open_recipe(recipe_id)`
- `create_recipe(form_data)`
- `update_draft(recipe_id, version_id, params)`
- `publish(recipe_id, version_id)`

状态字段建议：

- `recipes`
- `selected_recipe`
- `selected_version`
- `is_loading`
- `last_error`

### `DxfViewModel`

建议公开：

- `select_file(path)`
- `upload_selected_file()`
- `plan_current_file()`
- `execute_current_plan()`
- `refresh_job_status()`

状态字段建议：

- `selected_file`
- `upload_result`
- `plan_result`
- `execution_job`
- `progress`
- `last_error`

## 错误处理建议

建议 HMI 侧统一定义：

- `ProtocolError`
- `ApplicationError`
- `HardwareError`
- `InfrastructureError`
- `TransportError`

转换链：

```text
TCP error payload
    -> error_codes.py
        -> typed exception / typed UI error
            -> ViewModel
                -> Page toast / dialog / inline message
```

原则：

- 页面不直接判断错误码字符串
- 由 client 或 error mapper 统一翻译

## 与 control-core 的边界

`control-core` 只需要保证：

- method 名稳定
- JSON 信封稳定
- 错误码稳定
- 事件名稳定

`hmi-client` 负责：

- client 实现
- DTO / ViewModel
- 页面行为
- 本地交互体验

## 推荐落地顺序

1. 先实现 `transport` + `protocol`
2. 再实现 `BaseClient`
3. 再实现 `SystemClient` / `MotionClient`
4. 然后补 `RecipeClient` / `DxfClient` / `DispensingClient`
5. 最后补 `ViewModel` 和页面接线

## 最小首批落地集

如果只做第一批，建议优先落：

- `backend_manager.py`
- `tcp_transport.py`
- `base_client.py`
- `system_client.py`
- `motion_client.py`
- `event_client.py`
- `motion_view_model.py`
- `system_view_model.py`

这样可以最先打通：

- HMI 自动拉起后端
- 系统初始化
- 回零
- Jog
- 位置/状态刷新
- 事件订阅

一句话结论：

HMI 侧真正该稳定下来的，不是页面控件结构，而是 `transport -> client -> viewmodel` 这三层接口；只要这三层固定，页面和 TCP 契约都能独立演进。
