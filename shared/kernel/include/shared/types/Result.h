// Result.h - 统一错误处理类型 (Unified error handling type)
// Task: T012 - Phase 2 基础设施 - 共享类型系统
#pragma once

#include "Error.h"

#include <type_traits>
#include <utility>
#include <variant>

namespace Siligen {
namespace Shared {
namespace Types {

// Result<T> - 统一结果类型，用于函数返回值错误处理
// (Unified result type for function return value error handling)
//
// 使用示例 (Usage example):
//   Result<int> divide(int a, int b) {
//       if (b == 0) {
//           return Result<int>::Failure(Error(ErrorCode::INVALID_POSITION, "除数不能为零"));
//       }
//       return Result<int>::Success(a / b);
//   }
//
//   auto result = divide(10, 2);
//   if (result.IsSuccess()) {
//       std::cout << "结果: " << result.Value() << std::endl;
//   } else {
//       std::cout << result.GetError().ToString() << std::endl;
//   }
template <typename T>
class Result {
   private:
    std::variant<T, Error> value_;

   public:
    // 默认构造函数 - 创建成功结果 (Default constructor - create success result)
    Result() : value_(T{}) {}

    // 从值构造 (Construct from value)
    explicit Result(const T& value) : value_(value) {}
    explicit Result(T&& value) : value_(std::move(value)) {}

    // 从错误构造 (Construct from error)
    explicit Result(const Error& error) : value_(error) {}
    explicit Result(Error&& error) : value_(std::move(error)) {}

    // 拷贝构造和拷贝赋值 (Copy constructor and copy assignment)
    Result(const Result&) = default;
    Result& operator=(const Result&) = default;

    // 移动构造和移动赋值 (Move constructor and move assignment)
    Result(Result&&) noexcept = default;
    Result& operator=(Result&&) noexcept = default;

    // 判断是否成功 (Check if successful)
    bool IsSuccess() const {
        return std::holds_alternative<T>(value_);
    }

    // 判断是否失败 (Check if failed)
    bool IsError() const {
        return std::holds_alternative<Error>(value_);
    }

    // 获取值 (常量版本) - Get value (const version)
    // 注意: 调用前应检查 IsSuccess()
    const T& Value() const {
        return std::get<T>(value_);
    }

    // 获取值 (可变版本) - Get value (mutable version)
    // 注意: 调用前应检查 IsSuccess()
    T& Value() {
        return std::get<T>(value_);
    }

    // 获取值或默认值 (Get value or default)
    T ValueOr(const T& default_value) const {
        if (IsSuccess()) {
            return std::get<T>(value_);
        }
        return default_value;
    }

    // 获取错误 (Get error)
    // 注意: 调用前应检查 IsError()
    const Error& GetError() const {
        return std::get<Error>(value_);
    }

    // 创建成功结果 (Create success result)
    static Result<T> Success(const T& value) {
        return Result<T>(value);
    }

    static Result<T> Success(T&& value) {
        return Result<T>(std::move(value));
    }

    // 创建失败结果 (Create failure result)
    static Result<T> Failure(const Error& error) {
        return Result<T>(error);
    }

    static Result<T> Failure(Error&& error) {
        return Result<T>(std::move(error));
    }

    // 运算符重载 - 可用于if语句
    // Operator overload - can be used in if statements
    explicit operator bool() const {
        return IsSuccess();
    }

    // Map操作 - 转换成功值
    // Map operation - transform success value
    template <typename F>
    auto Map(F&& func) -> Result<decltype(func(std::declval<T>()))> {
        using U = decltype(func(std::declval<T>()));

        if (IsSuccess()) {
            return Result<U>::Success(func(Value()));
        }
        return Result<U>::Failure(GetError());
    }

    // FlatMap操作 - 链式调用
    // FlatMap operation - chain calls
    template <typename F>
    auto FlatMap(F&& func) -> decltype(func(std::declval<T>())) {
        using ResultType = decltype(func(std::declval<T>()));

        if (IsSuccess()) {
            return func(Value());
        }
        return ResultType::Failure(GetError());
    }
};

// Result<void> - void类型特化
// Result<void> - specialization for void type
template <>
class Result<void> {
   private:
    Error error_{};
    bool is_success_ = true;

   public:
    // 默认构造函数 - 创建成功结果
    // cppcheck-suppress functionStatic ; constructors cannot be static.
    Result() : error_(), is_success_(true) {}

    // 从错误构造
    // cppcheck-suppress functionStatic ; constructors cannot be static.
    explicit Result(const Error& error) : error_(error), is_success_(false) {}
    // cppcheck-suppress functionStatic ; constructors cannot be static.
    explicit Result(Error&& error) : error_(std::move(error)), is_success_(false) {}

    // 拷贝构造和拷贝赋值
    Result(const Result&) = default;
    Result& operator=(const Result&) = default;

    // 移动构造和移动赋值
    Result(Result&&) noexcept = default;
    Result& operator=(Result&&) noexcept = default;

    // 判断是否成功
    bool IsSuccess() const {
        return is_success_;
    }

    // 判断是否失败
    bool IsError() const {
        return !is_success_;
    }

    // 获取错误
    const Error& GetError() const {
        return error_;
    }

    // 创建成功结果
    static Result<void> Success() {
        return Result<void>();
    }

    // 创建失败结果
    static Result<void> Failure(const Error& error) {
        return Result<void>(error);
    }

    static Result<void> Failure(Error&& error) {
        return Result<void>(std::move(error));
    }

    // 运算符重载
    explicit operator bool() const {
        return IsSuccess();
    }

    // FlatMap操作 - 链式调用
    template <typename F>
    auto FlatMap(F&& func) -> decltype(func()) {
        using ResultType = decltype(func());

        if (IsSuccess()) {
            return func();
        }
        return ResultType::Failure(error_);
    }
};

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
