// 测试程序公共工具
#pragma once
#include "shared/types/Types.h"

#include <chrono>
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace TestHelper {

inline void InitConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

inline void PrintHeader(const char* title) {
    std::cout << "\n========================================\n  " << title
              << "\n========================================" << std::endl;
}

inline void PrintError(int code) {
    std::cout << "[ERROR] 连接失败: " << code << std::endl;
    const char* hint = code == -6   ? "检查: 24V电源/网络配置/IP冲突"
                       : code == -7 ? "检查: 控制卡启动/防火墙/网线"
                                    : nullptr;
    if (hint) std::cout << "[诊断] " << hint << std::endl;
}

// 硬件相关工具函数已移至基础设施层
// 以下为共享内核通用工具函数，保持零依赖特性

inline void WaitExit() {
    const char* ni = std::getenv("NONINTERACTIVE");
    if (ni && std::string(ni) == "1") {
        std::cout << "[INFO] 非交互模式，自动退出" << std::endl;
    } else {
        std::cout << "\n按任意键退出..." << std::endl;
        system("pause");
    }
}

inline void PrintFatalError(const char* type, const char* msg = nullptr) {
    std::cerr << "\n========================================\n  [FATAL] " << type
              << "\n========================================" << std::endl;
    if (msg) std::cerr << msg << std::endl;
    std::cout << "\n按任意键退出..." << std::endl;
    system("pause");
}

// Y2测试专用方法
inline void PrintInfo(const char* message) {
    std::cout << "[INFO] " << message << std::endl;
}

inline void PrintSuccess(const char* message) {
    std::cout << "[SUCCESS] " << message << std::endl;
}

inline void PrintPass(const char* testName) {
    std::cout << "[PASS] " << testName << std::endl;
}

inline void PrintFail(const char* testName) {
    std::cout << "[FAIL] " << testName << std::endl;
}

inline bool AskConfirmation(const char* question) {
    const char* ni = std::getenv("NONINTERACTIVE");
    if (ni && std::string(ni) == "1") {
        std::cout << "[INFO] 非交互模式，自动确认: " << question << std::endl;
        return true;
    }
    std::cout << "\n" << question << " (y/n): ";
    char answer;
    std::cin >> answer;
    return (answer == 'y' || answer == 'Y');
}

inline void PrintErrorWithCode(const char* module, const char* message, int code) {
    std::cout << "[ERROR] " << module << ": " << message << " (错误码: " << code << ")" << std::endl;
}

}  // namespace TestHelper

