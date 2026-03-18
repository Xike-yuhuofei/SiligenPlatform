# Adapters Layer (适配器层)

## 目录

本层包含外部接口适配器，负责与外部世界的交互。

### tcp/
TCP 服务器适配器，作为当前阶段唯一外部程序化入口，供 HMI 调用。

## 依赖原则

- **依赖** shared层的接口
- **使用** application层的用例
- **不直接依赖** domain或infrastructure层
- 所有业务逻辑通过用例调用

## 命名空间

```cpp
namespace Siligen::Adapters {
    namespace TCP { }   // TCP适配器
}
```
