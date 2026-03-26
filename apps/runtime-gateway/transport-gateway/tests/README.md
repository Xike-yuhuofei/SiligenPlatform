# transport-gateway 测试

```powershell
python apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py
```

覆盖点：

- canonical dispatcher 注册的方法名是否与 `shared/contracts/application/` 一致
- `apps/runtime-gateway/` 宿主是否只通过 `transport-gateway` 的公开 builder/host 装配服务
- legacy alias 与桥接入口是否已经删除且不会回流到 canonical target
