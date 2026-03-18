# 兼容性测试

```powershell
python D:\Projects\SiligenSuite\packages\application-contracts\tests\test_protocol_compatibility.py
```

测试覆盖：

- HMI `protocol.py` 中实际调用的方法名是否都已进入契约目录
- `packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp` 中实际注册的方法名是否与契约目录一致
- fixtures 的 envelope 结构是否符合当前协议事实
- 已知兼容差异是否被显式记录
