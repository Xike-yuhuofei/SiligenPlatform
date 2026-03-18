# MultiCard DLL 与 C++ 智能指针兼容性问题

日期: 2025-11-22

## 问题描述

`test_software_trigger.exe` 闪退，程序在调用 `MC_Open()` 时崩溃，SEH 异常捕获也无效。

## 根本原因

使用 `std::make_shared<MultiCard>()` 在堆上创建对象与 MultiCard.dll 的内存管理不兼容。

## 症状

- 程序启动后立即崩溃
- iostream 输出数字时异常（DLL 破坏了 iostream 内部状态）
- SEH 保护无法捕获异常

## 对比

```cpp
// 崩溃 - test_software_trigger.cpp (原)
auto multi_card = std::make_shared<MultiCard>();
multi_card->MC_Open(...);

// 正常 - test_mc_lnxy_class.cpp
MultiCard card;
card.MC_Open(...);
```

## 解决方案

MultiCard 对象必须在栈上创建：

```cpp
// 正确用法
MultiCard card;  // 栈上创建
card.MC_Open(CARD_NUM, (char*)LOCAL_IP, 0, (char*)CARD_IP, 0);

// 如需 shared_ptr（如传递给 TrajectoryExecutor），使用空删除器
auto multi_card = std::shared_ptr<MultiCard>(&card, [](MultiCard*){});
```

## 规则

所有使用 MultiCard.dll 的代码必须遵循 `test_mc_lnxy_class.cpp` 的模式，避免使用智能指针直接管理 MultiCard 对象。

## 诊断技巧

- 用 `fprintf`/`fflush` 替代 iostream 可避免 DLL 干扰输出
- 参考已有工作代码作为可靠模板
