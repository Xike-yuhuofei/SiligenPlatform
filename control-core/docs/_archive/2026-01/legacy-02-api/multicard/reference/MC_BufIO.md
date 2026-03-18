# MC_BufIO

## 功能

在缓冲区中插入一个IO操作。

## 函数原型

```cpp
short __stdcall MC_BufIO(short ConnectNo, short Crd, short IoType, short Mask, short Value);
```

## 参数

- `ConnectNo`：连接号。
- `Crd`：坐标系号。
- `IoType`：IO类型，0-DI，1-DO。
- `Mask`：IO掩码。
- `Value`：IO值。

## 返回值

- `0`：成功。
- `非0`：失败，返回错误码。
