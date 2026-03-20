#pragma once

// 定义数学常量以支持M_PI等
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

// ============================================================
// Siligen 预编译头文件 - 六边形架构优化版本
// 目标：减少编译时间，统一项目标准库包含
// 更新：2025-12-11 - 支持target_precompile_headers()
// 性能：预计减少20-40%编译时间
// ============================================================

// C++ 标准库 - 核心容器
#include <array>
#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// C++ 标准库 - 智能指针和工具
#include <algorithm>
#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

// C++ 标准库 - 字符串和IO流
#include <fstream>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>

// C++ 标准库 - 数值和数学
#include <bitset>
#include <cmath>
#include <complex>
#include <limits>
#include <numeric>
#include <ratio>
#include <valarray>

// C++ 标准库 - 异常和错误处理
#include <exception>
#include <stdexcept>
#include <system_error>
#include <typeindex>
#include <typeinfo>

// C++ 标准库 - 多线程和并发
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>
// #include <shared_mutex>  // C++17特性，某些编译器可能不支持

// C++ 标准库 - 时间和日期
#include <chrono>
#include <ctime>

// C++ 标准库 - 类型特征
#include <type_traits>
#include <typeinfo>

// C++ 标准库 - 迭代器和范围
#include <iterator>
// #include <ranges>  // C++20特性，暂时注释

// C++ 标准库 - 随机数生成
#include <random>

// C++ 标准库 - 编码和本地化
#include <codecvt>
#include <locale>

// C 标准库兼容
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// ============================================================
// Windows平台特定 - 优化版本
// ============================================================
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// 防止 windows.h 包含 winsock.h（避免与 winsock2.h 冲突）
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif
// 必须在 windows.h 之前包含 winsock2.h 以避免冲突
#ifndef _WINSOCK2API_
#include <winsock2.h>
#endif
#ifndef _WS2TCPIP_H_
#include <ws2tcpip.h>
#endif
#include <errhandlingapi.h>
#include <fileapi.h>
#include <handleapi.h>
#include <minwinbase.h>
#include <minwindef.h>
#include <process.h>
#include <synchapi.h>
#include <tchar.h>
#include <winbase.h>
#include <windef.h>
#include <windows.h>
#include <winnt.h>
#include <winuser.h>

// 取消定义Windows宏以避免污染C++代码
#ifdef GetMessage
#undef GetMessage
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

// 自动链接常用Windows库
#pragma comment(lib, "user32")
#pragma comment(lib, "kernel32")
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "advapi32")
#endif

// ============================================================
// Siligen项目核心类型 (仅稳定接口)
// ============================================================
#include "shared/types/CMPTypes.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"

// ============================================================
// 通用工具函数 (inline实现)
// ============================================================
namespace Siligen::Common {

// 安全删除指针
template <typename T>
inline void SafeDelete(T*& ptr) {
    delete ptr;
    ptr = nullptr;
}

// 安全删除数组
template <typename T>
inline void SafeDeleteArray(T*& ptr) {
    delete[] ptr;
    ptr = nullptr;
}

// 获取数组长度 (编译时计算)
template <typename T, size_t N>
constexpr size_t ArraySize(const T (&)[N]) noexcept {
    return N;
}

// 字符串转换辅助
inline std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
#ifdef _WIN32
    int size_needed = WideCharToMultiByte(CP_UTF8,
                                          0,
                                          wstr.data(),
                                          static_cast<int>(wstr.size()),
                                          nullptr,
                                          0,
                                          nullptr,
                                          nullptr);
    if (size_needed <= 0) {
        return "";
    }
    std::string result(static_cast<size_t>(size_needed), '\0');
    WideCharToMultiByte(CP_UTF8,
                        0,
                        wstr.data(),
                        static_cast<int>(wstr.size()),
                        result.data(),
                        size_needed,
                        nullptr,
                        nullptr);
    return result;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
#endif
}

inline std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return L"";
#ifdef _WIN32
    int size_needed = MultiByteToWideChar(CP_UTF8,
                                          0,
                                          str.data(),
                                          static_cast<int>(str.size()),
                                          nullptr,
                                          0);
    if (size_needed <= 0) {
        return L"";
    }
    std::wstring result(static_cast<size_t>(size_needed), L'\0');
    MultiByteToWideChar(CP_UTF8,
                        0,
                        str.data(),
                        static_cast<int>(str.size()),
                        result.data(),
                        size_needed);
    return result;
#else
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
#endif
}

// RAII锁助手
template <typename Mutex>
class ScopedLock {
   public:
    explicit ScopedLock(Mutex& m) : mutex_(m) {
        mutex_.lock();
    }
    ~ScopedLock() {
        mutex_.unlock();
    }

   private:
    Mutex& mutex_;
};

}  // namespace Siligen::Common

// ============================================================
// 编译器优化和警告控制
// ============================================================
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4251)  // DLL interface warning
#pragma warning(disable : 4275)  // DLL interface warning for base classes
#pragma warning(disable : 4661)  // no suitable definition provided for explicit template instantiation request
#pragma warning(disable : 4244)  // conversion from 'type1' to 'type2', possible loss of data
#pragma warning(disable : 4267)  // 'var': conversion from 'size_t' to 'type', possible loss of data
#endif

// 版本标识
#define SILIGEN_PCH_VERSION 20251211

// ============================================================
// 预编译头使用说明和最佳实践
// ============================================================
//
// 1. 自动应用: 通过target_precompile_headers()应用到各个目标
// 2. 加速效果: 减少20-40%编译时间 (标准库heavy项目可达50%+)
// 3. 适用目标: 核心库 + 大型可执行文件
// 4. 维护原则:
//    - 只包含稳定的、频繁使用的头文件
//    - 标准库头文件优先 (被广泛使用)
//    - 避免包含项目头文件 (变化频繁)
//    - 定期审查未使用的包含
// 5. 性能监控:
//    - 编译时间对比: cmake --build . --timings
//    - PCH命中率监控构建日志
//
// ============================================================

// 恢复警告设置
#ifdef _WIN32
#pragma warning(pop)
#endif
