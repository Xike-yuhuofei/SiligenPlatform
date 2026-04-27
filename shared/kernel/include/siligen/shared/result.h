#pragma once

#include "siligen/shared/error.h"

#include <type_traits>
#include <utility>
#include <variant>

namespace Siligen::SharedKernel {

template <typename T>
class Result {
   public:
    Result() : value_(T{}) {}
    explicit Result(const T& value) : value_(value) {}
    explicit Result(T&& value) : value_(std::move(value)) {}
    explicit Result(const Error& error) : value_(error) {}
    explicit Result(Error&& error) : value_(std::move(error)) {}

    Result(const Result&) = default;
    Result& operator=(const Result&) = default;
    Result(Result&&) noexcept = default;
    Result& operator=(Result&&) noexcept = default;

    [[nodiscard]] bool IsSuccess() const { return std::holds_alternative<T>(value_); }
    [[nodiscard]] bool IsError() const { return std::holds_alternative<Error>(value_); }

    [[nodiscard]] const T& Value() const { return std::get<T>(value_); }
    [[nodiscard]] T& Value() { return std::get<T>(value_); }
    [[nodiscard]] T ValueOr(const T& fallback) const { return IsSuccess() ? std::get<T>(value_) : fallback; }
    [[nodiscard]] const Error& GetError() const { return std::get<Error>(value_); }

    static Result<T> Success(const T& value) { return Result<T>(value); }
    static Result<T> Success(T&& value) { return Result<T>(std::move(value)); }
    static Result<T> Failure(const Error& error) { return Result<T>(error); }
    static Result<T> Failure(Error&& error) { return Result<T>(std::move(error)); }

    explicit operator bool() const { return IsSuccess(); }

    template <typename F>
    auto Map(F&& func) -> Result<decltype(func(std::declval<T>()))> {
        using U = decltype(func(std::declval<T>()));
        if (IsSuccess()) {
            return Result<U>::Success(func(Value()));
        }
        return Result<U>::Failure(GetError());
    }

    template <typename F>
    auto FlatMap(F&& func) -> decltype(func(std::declval<T>())) {
        using ResultType = decltype(func(std::declval<T>()));
        if (IsSuccess()) {
            return func(Value());
        }
        return ResultType::Failure(GetError());
    }

   private:
    std::variant<T, Error> value_;
};

template <>
class Result<void> {
   public:
    // cppcheck-suppress functionStatic ; constructors cannot be static.
    Result() : is_success_(true) {}
    // cppcheck-suppress functionStatic ; constructors cannot be static.
    explicit Result(const Error& error) : error_(error), is_success_(false) {}
    // cppcheck-suppress functionStatic ; constructors cannot be static.
    explicit Result(Error&& error) : error_(std::move(error)), is_success_(false) {}

    Result(const Result&) = default;
    Result& operator=(const Result&) = default;
    Result(Result&&) noexcept = default;
    Result& operator=(Result&&) noexcept = default;

    [[nodiscard]] bool IsSuccess() const { return is_success_; }
    [[nodiscard]] bool IsError() const { return !is_success_; }
    [[nodiscard]] const Error& GetError() const { return error_; }

    static Result<void> Success() { return Result<void>(); }
    static Result<void> Failure(const Error& error) { return Result<void>(error); }
    static Result<void> Failure(Error&& error) { return Result<void>(std::move(error)); }

    explicit operator bool() const { return IsSuccess(); }

    template <typename F>
    auto FlatMap(F&& func) -> decltype(func()) {
        using ResultType = decltype(func());
        if (IsSuccess()) {
            return func();
        }
        return ResultType::Failure(error_);
    }

   private:
    Error error_{};
    bool is_success_ = true;
};

using VoidResult = Result<void>;

}  // namespace Siligen::SharedKernel
