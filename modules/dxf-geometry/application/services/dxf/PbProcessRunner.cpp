#include "application/services/dxf/PbProcessRunner.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Siligen::Application::Services::DXF::detail {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

constexpr const char* kModuleName = "DxfPbPreparationService";

std::string QuoteArg(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) {
        return value;
    }

    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (char c : value) {
        if (c == '"') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::string BuildCommandSummary(const std::vector<std::string>& args) {
    if (args.empty()) {
        return "<empty>";
    }

    std::ostringstream out;
    const size_t limit = std::min<size_t>(args.size(), 8);
    for (size_t index = 0; index < limit; ++index) {
        if (index > 0) {
            out << ' ';
        }
        out << QuoteArg(args[index]);
    }
    if (args.size() > limit) {
        out << " ...(+" << (args.size() - limit) << " args)";
    }
    return out.str();
}

#ifdef _WIN32
Result<int> RunProcess(const std::vector<std::string>& args) {
    if (args.empty()) {
        return Result<int>::Failure(Error(ErrorCode::COMMAND_FAILED, "外部命令参数为空", kModuleName));
    }

    std::ostringstream command_stream;
    for (size_t index = 0; index < args.size(); ++index) {
        if (index > 0) {
            command_stream << ' ';
        }
        command_stream << QuoteArg(args[index]);
    }

    std::string command = command_stream.str();
    std::vector<char> mutable_command(command.begin(), command.end());
    mutable_command.push_back('\0');

    STARTUPINFOA startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};
    const BOOL created = ::CreateProcessA(nullptr,
                                          mutable_command.data(),
                                          nullptr,
                                          nullptr,
                                          FALSE,
                                          CREATE_NO_WINDOW,
                                          nullptr,
                                          nullptr,
                                          &startup_info,
                                          &process_info);
    if (!created) {
        const DWORD err = ::GetLastError();
        return Result<int>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "启动PB生成进程失败: winerr=" + std::to_string(static_cast<unsigned long>(err)),
                  kModuleName));
    }

    const DWORD wait_result = ::WaitForSingleObject(process_info.hProcess, INFINITE);
    if (wait_result == WAIT_FAILED) {
        const DWORD err = ::GetLastError();
        ::CloseHandle(process_info.hThread);
        ::CloseHandle(process_info.hProcess);
        return Result<int>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "等待PB生成进程失败: winerr=" + std::to_string(static_cast<unsigned long>(err)),
                  kModuleName));
    }

    DWORD exit_code = 1;
    if (!::GetExitCodeProcess(process_info.hProcess, &exit_code)) {
        const DWORD err = ::GetLastError();
        ::CloseHandle(process_info.hThread);
        ::CloseHandle(process_info.hProcess);
        return Result<int>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "读取PB生成进程退出码失败: winerr=" + std::to_string(static_cast<unsigned long>(err)),
                  kModuleName));
    }

    ::CloseHandle(process_info.hThread);
    ::CloseHandle(process_info.hProcess);
    return Result<int>::Success(static_cast<int>(exit_code));
}
#else
Result<int> RunProcess(const std::vector<std::string>& args) {
    if (args.empty()) {
        return Result<int>::Failure(Error(ErrorCode::COMMAND_FAILED, "外部命令参数为空", kModuleName));
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        return Result<int>::Failure(Error(ErrorCode::COMMAND_FAILED, "fork 失败", kModuleName));
    }

    if (pid == 0) {
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        ::execvp(argv.front(), argv.data());
        _exit(127);
    }

    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) {
        return Result<int>::Failure(Error(ErrorCode::COMMAND_FAILED, "waitpid 失败", kModuleName));
    }

    if (WIFEXITED(status)) {
        return Result<int>::Success(WEXITSTATUS(status));
    }
    if (WIFSIGNALED(status)) {
        return Result<int>::Success(128 + WTERMSIG(status));
    }
    return Result<int>::Success(status);
}
#endif

}  // namespace

Result<void> PbProcessRunner::ValidateOutput(const std::filesystem::path& pb_path) const {
    std::error_code ec;
    if (!std::filesystem::exists(pb_path, ec)) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, "PB output not found: " + pb_path.string(), kModuleName));
    }

    const auto pb_size = std::filesystem::file_size(pb_path, ec);
    if (ec || pb_size == 0) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_FORMAT_INVALID, "PB output is empty: " + pb_path.string(), kModuleName));
    }

    return Result<void>::Success();
}

Result<void> PbProcessRunner::Run(const std::vector<std::string>& command_args,
                                  const std::filesystem::path& pb_path) const {
    std::error_code remove_ec;
    std::filesystem::remove(pb_path, remove_ec);

    SILIGEN_LOG_INFO("执行PB生成命令: " + BuildCommandSummary(command_args));
    auto run_result = RunProcess(command_args);
    if (run_result.IsError()) {
        return Result<void>::Failure(run_result.GetError());
    }

    const int exit_code = run_result.Value();
    if (exit_code != 0) {
        return Result<void>::Failure(
            Error(ErrorCode::COMMAND_FAILED,
                  "PB generator failed: exit_code=" + std::to_string(exit_code),
                  kModuleName));
    }

    auto validation_result = ValidateOutput(pb_path);
    if (validation_result.IsError()) {
        return validation_result;
    }

    SILIGEN_LOG_INFO("PB generated: " + pb_path.string());
    return Result<void>::Success();
}

}  // namespace Siligen::Application::Services::DXF::detail
