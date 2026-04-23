#include "CommandHandlers.h"
#include "CommandHandlersInternal.h"

#include "runtime_execution/application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"

#include <chrono>
#include <thread>

namespace Siligen::Adapters::CLI {

using Siligen::Application::CommandLineConfig;
using Siligen::Application::CommandType;
using Siligen::Application::UseCases::Dispensing::Valve::PurgeDispenserRequest;
using Siligen::Application::UseCases::Dispensing::Valve::PurgeDispenserResponse;
using Siligen::Application::UseCases::Dispensing::Valve::ValveCommandUseCase;
using Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

Result<void> CLICommandHandlers::WaitForPurgeKey(const CommandLineConfig& config, bool wait_for_release) {
    auto monitoring_usecase = container_->Resolve<MotionMonitoringUseCase>();
    if (!monitoring_usecase) {
        return Result<void>::Failure(
            Error(ErrorCode::SERVICE_NOT_REGISTERED, "无法解析 IO 监控用例", "CLI"));
    }

    if (config.purge_key_channel < 0 || config.purge_key_channel > 15) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "排胶键通道号必须在 0-15 范围内", "CLI"));
    }

    const int16 channel = static_cast<int16>(config.purge_key_channel);
    std::cout << "等待排胶键(X" << config.purge_key_channel << ")"
              << (wait_for_release ? "释放" : "触发") << "..." << std::endl;

    bool last_active = false;
    bool seen_active = wait_for_release;
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        auto status_result = monitoring_usecase->ReadDigitalInputStatus(channel);
        if (status_result.IsError()) {
            return Result<void>::Failure(status_result.GetError());
        }

        const bool active = status_result.Value().signal_active;
        if (!wait_for_release) {
            if (active && !last_active) {
                return Result<void>::Success();
            }
        } else {
            if (active) {
                seen_active = true;
            }
            if (seen_active && !active) {
                return Result<void>::Success();
            }
        }

        last_active = active;
        if (config.timeout_ms > 0) {
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (elapsed_ms > config.timeout_ms) {
                return Result<void>::Failure(
                    Error(ErrorCode::TIMEOUT, "等待排胶键超时", "CLI"));
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int CLICommandHandlers::HandleDispenser(const CommandLineConfig& config) {
    auto ensure = EnsureConnected(config);
    if (ensure.IsError()) {
        PrintError(ensure.GetError());
        return 1;
    }

    auto usecase = container_->Resolve<ValveCommandUseCase>();
    if (!usecase) {
        std::cout << "无法解析阀控制用例" << std::endl;
        return 1;
    }

    if (config.command == CommandType::DISPENSER_START) {
        Siligen::Domain::Dispensing::Ports::DispenserValveParams params;
        params.count = config.dispenser_count;
        params.intervalMs = config.dispenser_interval_ms;
        params.durationMs = config.dispenser_duration_ms;
        if (!params.IsValid()) {
            std::cout << "点胶参数无效: " << params.GetValidationError() << std::endl;
            return 1;
        }

        auto result = usecase->StartDispenser(params);
        if (result.IsError()) {
            PrintError(result.GetError());
            return 1;
        }

        std::cout << "点胶阀已启动，状态: " << DispenserStatusToString(result.Value().status) << std::endl;
        return 0;
    }

    if (config.command == CommandType::DISPENSER_PURGE) {
        PurgeDispenserRequest request;
        request.wait_for_completion = false;
        request.manage_supply = true;
        request.timeout_ms = static_cast<uint32>(config.timeout_ms);

        if (!request.Validate()) {
            std::cout << "排胶参数无效: " << request.GetValidationError() << std::endl;
            return 1;
        }

        const auto start_purge = [&]() -> Result<PurgeDispenserResponse> {
            return usecase->PurgeDispenser(request);
        };

        const auto stop_and_close = [&]() -> Result<void> {
            auto stop_result = usecase->StopDispenser();
            if (stop_result.IsError()) {
                return Result<void>::Failure(stop_result.GetError());
            }

            auto close_result = usecase->CloseSupplyValve();
            if (close_result.IsError()) {
                return Result<void>::Failure(close_result.GetError());
            }

            return Result<void>::Success();
        };

        if (config.wait_purge_key) {
            auto wait_press = WaitForPurgeKey(config, false);
            if (wait_press.IsError()) {
                PrintError(wait_press.GetError());
                return 1;
            }

            auto start_result = start_purge();
            if (start_result.IsError()) {
                PrintError(start_result.GetError());
                return 1;
            }

            auto release_result = WaitForPurgeKey(config, true);
            if (release_result.IsError()) {
                PrintError(release_result.GetError());
                auto stop_result = stop_and_close();
                if (stop_result.IsError()) {
                    PrintError(stop_result.GetError());
                }
                return 1;
            }

            auto stop_result = stop_and_close();
            if (stop_result.IsError()) {
                PrintError(stop_result.GetError());
                return 1;
            }

            std::cout << "排胶已停止" << std::endl;
            return 0;
        }

        if (config.wait_for_completion) {
            if (config.timeout_ms <= 0) {
                std::cout << "排胶时长必须大于 0ms" << std::endl;
                return 1;
            }

            auto start_result = start_purge();
            if (start_result.IsError()) {
                PrintError(start_result.GetError());
                return 1;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(config.timeout_ms));

            auto stop_result = stop_and_close();
            if (stop_result.IsError()) {
                PrintError(stop_result.GetError());
                return 1;
            }

            std::cout << "排胶完成" << std::endl;
            return 0;
        }

        auto start_result = start_purge();
        if (start_result.IsError()) {
            PrintError(start_result.GetError());
            return 1;
        }

        std::cout << "排胶已启动（需手动停止并关闭供胶阀）" << std::endl;
        return 0;
    }

    if (config.command == CommandType::DISPENSER_STOP) {
        auto result = usecase->StopDispenser();
        if (result.IsError()) {
            PrintError(result.GetError());
            return 1;
        }
        std::cout << "点胶阀已停止" << std::endl;
        return 0;
    }

    if (config.command == CommandType::DISPENSER_PAUSE) {
        auto result = usecase->PauseDispenser();
        if (result.IsError()) {
            PrintError(result.GetError());
            return 1;
        }
        std::cout << "点胶阀已暂停" << std::endl;
        return 0;
    }

    if (config.command == CommandType::DISPENSER_RESUME) {
        auto result = usecase->ResumeDispenser();
        if (result.IsError()) {
            PrintError(result.GetError());
            return 1;
        }
        std::cout << "点胶阀已恢复" << std::endl;
        return 0;
    }

    std::cout << "未知点胶操作" << std::endl;
    return 1;
}

int CLICommandHandlers::HandleSupply(const CommandLineConfig& config) {
    auto ensure = EnsureConnected(config);
    if (ensure.IsError()) {
        PrintError(ensure.GetError());
        return 1;
    }

    auto usecase = container_->Resolve<ValveCommandUseCase>();
    if (!usecase) {
        std::cout << "无法解析阀控制用例" << std::endl;
        return 1;
    }

    if (config.command == CommandType::SUPPLY_OPEN) {
        auto result = usecase->OpenSupplyValve();
        if (result.IsError()) {
            PrintError(result.GetError());
            return 1;
        }
        std::cout << "供胶阀已打开" << std::endl;
        return 0;
    }

    auto result = usecase->CloseSupplyValve();
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << "供胶阀已关闭" << std::endl;
    return 0;
}

}  // namespace Siligen::Adapters::CLI
