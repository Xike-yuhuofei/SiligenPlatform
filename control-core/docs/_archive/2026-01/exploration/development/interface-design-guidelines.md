# 接口设计规范 (Interface Design Guidelines)

**版本:** 1.0
**生效日期:** 2026-01-07
**适用范围:** 所有 Domain Port 接口

---

## 1. 返回值类型规范

### 规则1.1: 所有端口接口方法必须返回Result<T>

**正确:**
```cpp
Result<void> MoveToPosition(const Point2D& target);
Result<int> GetAxisStatus(int32 axis_id);
```

**错误:**
```cpp
bool MoveToPosition(const Point2D& target);  // ❌ 无法传递错误信息
int GetAxisStatus(int32 axis_id);           // ❌ 无错误处理
```

**例外:**
- 简单getter(不会失败): `int32 GetId() const;`
- 状态查询(预期不会失败): `bool IsConnected() const;`

---

### 规则1.2: 参数传递优先使用const引用

**正确:**
```cpp
Result<void> Process(const Point2D& point);
Result<void> Configure(const MotionConfig& config);
```

**错误:**
```cpp
Result<void> Process(Point2D point);        // ❌ 不必要的拷贝
Result<void> Process(Point2D* point);       // ❌ 使用指针,有nullptr风险
```

**例外:**
- 基础类型(<= 8字节): `void SetValue(int32 value);`
- 需要修改参数: `void UpdatePoint(Point2D& point);`

---

### 规则1.3: 命名规范

#### 方法命名 - 动词开头,驼峰命名
```cpp
Result<void> Connect(const std::string& address);
Result<void> Disconnect();
Result<Point2D> GetCurrentPosition();
```

#### 参数命名 - 小写下划线
```cpp
void SetTargetPosition(int32 target_x, int32 target_y);
```

#### 类型命名 - 大驼峰
```cpp
struct MotionConfig { };
enum class AxisState { };
```

---

## 2. 接口隔离原则(ISP)

### 规则2.1: 单一职责接口

**推荐:**
```cpp
// 细粒度接口
class IAxisControlPort {
    virtual Result<void> EnableAxis(int32 axis_id) = 0;
    virtual Result<void> DisableAxis(int32 axis_id) = 0;
};

class IPositionControlPort {
    virtual Result<void> MoveTo(const Point2D& target) = 0;
    virtual Result<void> Stop() = 0;
};
```

**避免:**
```cpp
// 上帝接口(已废弃)
class LegacyMotionControllerPort {
    virtual Result<void> EnableAxis(...) = 0;
    virtual Result<void> MoveTo(...) = 0;
    virtual Result<void> Stop(...) = 0;
    // ... 50+ 个方法
};
```

---

## 3. 错误处理规范

### 规则3.1: 使用Result<T>而非异常

**正确:**
```cpp
Result<int> Divide(int a, int b) {
    if (b == 0) {
        return Result<int>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "除数不能为零")
        );
    }
    return Result<int>::Success(a / b);
}
```

**错误:**
```cpp
int Divide(int a, int b) {  // ❌ 抛出异常
    if (b == 0) {
        throw std::runtime_error("除数不能为零");
    }
    return a / b;
}
```

---

## 4. 文档注释规范

### 规则4.1: 所有公共接口必须有文档注释

**正确:**
```cpp
/**
 * @brief 移动到指定位置
 * @details 执行点到点运动,使用梯形速度曲线
 * @param target 目标位置(毫米)
 * @param velocity 运动速度(毫米/秒),范围[1, 1000]
 * @return Result<void> 成功返回Success,失败返回错误信息
 *         - ErrorCode::INVALID_PARAMETER 参数超出范围
 *         - ErrorCode::MOTION_FAILED 运动执行失败
 */
Result<void> MoveToPosition(const Point2D& target, float32 velocity);
```

**错误:**
```cpp
// ❌ 无文档注释
Result<void> MoveToPosition(const Point2D& target, float32 velocity);

// ❌ 注释不完整
// 移动到目标位置
Result<void> MoveToPosition(const Point2D& target, float32 velocity);
```

---

## 5. 接口版本控制

### 规则5.1: 废弃接口标记

**正确:**
```cpp
class LegacyMotionControllerPort {
    /**
     * @brief 移动到位置
     * @deprecated 请使用IPositionControlPort::MoveTo替代
     * @date 2026-01-07
     */
    [[deprecated("Use IPositionControlPort::MoveTo instead")]]
    virtual Result<void> MoveToPosition(const Point2D& target) = 0;
};
```

---

## 6. 接口设计检查清单

接口设计完成后,必须通过以下检查:

- [ ] 所有方法返回Result<T> (有合理例外)
- [ ] 参数传递使用const引用(符合规则1.2)
- [ ] 命名符合规范(规则1.3)
- [ ] 接口职责单一(规则2.1)
- [ ] 错误处理使用Result<T>(规则3.1)
- [ ] 有完整的文档注释(规则4.1)
- [ ] 编译无警告
- [ ] 有对应的单元测试

---

## 7. 常见错误示例

### 错误1: 返回bool的接口方法
```cpp
// ❌ 错误
virtual bool Connect(const std::string& address) = 0;

// ✅ 正确
virtual Result<void> Connect(const std::string& address) = 0;
```

### 错误2: 使用指针参数
```cpp
// ❌ 错误
virtual Result<void> Process(Point2D* point) = 0;

// ✅ 正确
virtual Result<void> Process(const Point2D& point) = 0;
```

### 错误3: 缺少文档注释
```cpp
// ❌ 错误
virtual Result<void> Initialize() = 0;

// ✅ 正确
/**
 * @brief 初始化系统
 * @details 执行必要的初始化操作,包括硬件连接和状态检查
 * @return Result<void> 成功返回Success,失败返回错误信息
 */
virtual Result<void> Initialize() = 0;
```

---

## 8. 参考资源

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [Hexagonal Architecture Pattern](https://alistair.cockburn.us/hexagonal-architecture/)
- [Interface Segregation Principle](https://en.wikipedia.org/wiki/Interface_segregation_principle)
- 项目架构文档: `.claude/rules/HEXAGONAL.md`
- 项目领域规则: `.claude/rules/DOMAIN.md`
