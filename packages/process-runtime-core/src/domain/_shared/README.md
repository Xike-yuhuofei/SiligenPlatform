# Domain 共享组件

**职责**: 提供领域层内部所有子域共享的值对象、规约模式等基础组件

## 目录结构

```
_shared/
└── (当前为空，预留给跨子域共享的值对象与规约)
```

## 使用原则

1. **仅包含纯领域概念**: 不依赖任何技术实现
2. **值对象不可变**: 一旦创建不可修改
3. **跨子域复用**: 多个子域都需要的值对象放在这里

## 命名空间

```cpp
namespace Siligen::Domain::Shared {
    namespace ValueObjects { ... }
    namespace Specifications { ... }
}
```
