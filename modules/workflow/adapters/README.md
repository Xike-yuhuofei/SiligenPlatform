# adapters

workflow 的外部依赖适配实现只允许收敛到此目录。

当前保留项：
- `persistence/`
- `messaging/`
- `projection_store/`

上述 3 个目录当前只保留 shell root，不再承载任何 legacy redundancy / bridge payload。

不再允许：
- recipe owner JSON serialization implementation
- recipe serializer deprecated forward headers
- DXF / geometry shadow concrete
- bridge-only adapter target
