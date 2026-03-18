# transport-gateway 测试

```powershell
python D:\Projects\SiligenSuite\packages\transport-gateway\tests\test_transport_gateway_compatibility.py
```

覆盖点：

- canonical dispatcher 注册的方法名是否与 `application-contracts` 一致
- `control-tcp-server` 薄入口是否只使用 `transport-gateway` 的公开 builder/host
- legacy alias shell 是否仍然转发到 canonical target
