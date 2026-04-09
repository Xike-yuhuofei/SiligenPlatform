# adapters

workflow 的外部依赖适配实现只允许收敛到此目录。

当前保留项：
- `infrastructure/adapters/redundancy/`：workflow recovery/redundancy port 的 JSON 持久化 concrete adapter。
- `include/infrastructure/adapters/redundancy/`：对外暴露 redundancy adapter 的最小 public header。

不再允许：
- recipe owner JSON serialization implementation
- recipe serializer deprecated forward headers
- DXF / geometry shadow concrete
- bridge-only adapter target
