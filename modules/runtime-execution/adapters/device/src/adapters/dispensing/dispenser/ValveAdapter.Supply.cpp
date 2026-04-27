// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "ValveAdapter"

#include "ValveAdapter.h"
#include "shared/interfaces/ILoggingService.h"
#include <chrono>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

using namespace Domain::Dispensing::Ports;
using namespace Shared::Types;

// ============================================================
// 供胶阀控制实现
// ============================================================

Result<SupplyValveState> ValveAdapter::OpenSupply() noexcept {
    try {
        std::lock_guard<std::mutex> lock(supply_mutex_);

        SILIGEN_LOG_INFO("OpenSupply: 开始打开供胶阀 Y" + std::to_string(supply_config_.do_bit_index));

        // 打印当前状态
        auto current_state = (supply_status_.state == SupplyValveState::Open ? "Open" :
                             supply_status_.state == SupplyValveState::Closed ? "Closed" : "Error");
        SILIGEN_LOG_INFO(std::string("OpenSupply: 当前状态: ") + current_state);

        // 调用 MultiCard API 设置扩展 DO（使用配置的位索引）
        SILIGEN_LOG_INFO("OpenSupply: 调用 MC_SetExtDoBit: cardIndex=" + std::to_string(supply_config_.do_card_index) +
                         ", bitIndex=" + std::to_string(supply_config_.do_bit_index) + ", value=1 (高电平)");

        int result = CallMC_SetExtDoBit(1);  // value=1 (打开)

        // 详细记录硬件返回值
        SILIGEN_LOG_INFO("OpenSupply: MC_SetExtDoBit 返回码: " + std::to_string(result) +
                         (result == 0 ? " (成功)" : " (失败)"));

        if (result != 0) {
            std::string error_msg = CreateErrorMessage(result, "OpenSupply");

            // 错误日志
            SILIGEN_LOG_ERROR("OpenSupply: 打开供胶阀失败");
            SILIGEN_LOG_ERROR(std::string("错误详情: ") + error_msg);
            SILIGEN_LOG_ERROR("硬件调用: MC_SetExtDoBit(cardIndex=" + std::to_string(supply_config_.do_card_index) +
                             ", bitIndex=" + std::to_string(supply_config_.do_bit_index) + ", value=1)");
            SILIGEN_LOG_ERROR("返回码: " + std::to_string(result));
            SILIGEN_LOG_ERROR("诊断建议: 检查硬件连接、电磁阀供电、控制卡初始化、端子接线");

            supply_status_.state = SupplyValveState::Error;
            supply_status_.errorMessage = error_msg;
            return Result<SupplyValveState>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        // 更新状态
        supply_status_.state = SupplyValveState::Open;
        supply_status_.errorMessage.clear();

        // 记录状态变更时间
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        supply_status_.lastChangeTime = static_cast<uint64>(timestamp);

        SILIGEN_LOG_INFO("OpenSupply: 供胶阀 Y" + std::to_string(supply_config_.do_bit_index) + " 打开成功");
        SILIGEN_LOG_INFO("OpenSupply: 新状态: Open, 时间戳: " + std::to_string(timestamp) + "ms");

        return Result<SupplyValveState>::Success(supply_status_.state);
    }
    catch (const std::exception& e) {
        SILIGEN_LOG_ERROR(std::string("OpenSupply: 异常: ") + e.what());

        supply_status_.state = SupplyValveState::Error;
        supply_status_.errorMessage = std::string("Exception: ") + e.what();
        return Result<SupplyValveState>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, supply_status_.errorMessage));
    }
}

Result<SupplyValveState> ValveAdapter::CloseSupplyInternal(const char* operation) noexcept {
    try {
        std::lock_guard<std::mutex> lock(supply_mutex_);

        SILIGEN_LOG_INFO(std::string(operation) + ": 开始关闭供胶阀 Y" + std::to_string(supply_config_.do_bit_index));

        auto current_state = (supply_status_.state == SupplyValveState::Open ? "Open" :
                               supply_status_.state == SupplyValveState::Closed ? "Closed" : "Error");
        SILIGEN_LOG_INFO(std::string(operation) + ": 当前状态: " + current_state);

        SILIGEN_LOG_INFO(std::string(operation) + ": 调用 MC_SetExtDoBit: cardIndex=" + std::to_string(supply_config_.do_card_index) +
                         ", bitIndex=" + std::to_string(supply_config_.do_bit_index) + ", value=0 (低电平)");

        int result = CallMC_SetExtDoBit(0);  // value=0 (关闭)

        SILIGEN_LOG_INFO(std::string(operation) + ": MC_SetExtDoBit 返回码: " + std::to_string(result) +
                         (result == 0 ? " (成功)" : " (失败)"));

        if (result != 0) {
            std::string error_msg = CreateErrorMessage(result, operation);

            SILIGEN_LOG_ERROR(std::string(operation) + ": 关闭供胶阀失败");
            SILIGEN_LOG_ERROR(std::string("错误详情: ") + error_msg);
            SILIGEN_LOG_ERROR("硬件调用: MC_SetExtDoBit(cardIndex=" + std::to_string(supply_config_.do_card_index) +
                             ", bitIndex=" + std::to_string(supply_config_.do_bit_index) + ", value=0)");
            SILIGEN_LOG_ERROR("返回码: " + std::to_string(result));
            SILIGEN_LOG_ERROR("诊断建议: 检查硬件连接、电磁阀供电、控制卡初始化、端子接线");

            supply_status_.state = SupplyValveState::Error;
            supply_status_.errorMessage = error_msg;
            return Result<SupplyValveState>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        supply_status_.state = SupplyValveState::Closed;
        supply_status_.errorMessage.clear();

        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        supply_status_.lastChangeTime = static_cast<uint64>(timestamp);

        SILIGEN_LOG_INFO(std::string(operation) + ": 供胶阀 Y" + std::to_string(supply_config_.do_bit_index) + " 关闭成功");
        SILIGEN_LOG_INFO(std::string(operation) + ": 新状态: Closed, 时间戳: " + std::to_string(timestamp) + "ms");

        return Result<SupplyValveState>::Success(supply_status_.state);
    }
    catch (const std::exception& e) {
        SILIGEN_LOG_ERROR(std::string(operation) + ": 异常: " + e.what());

        supply_status_.state = SupplyValveState::Error;
        supply_status_.errorMessage = std::string("Exception: ") + e.what();
        return Result<SupplyValveState>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, supply_status_.errorMessage));
    }
}

Result<SupplyValveState> ValveAdapter::CloseSupply() noexcept {
    return CloseSupplyInternal("CloseSupply");
}

Result<SupplyValveStatusDetail> ValveAdapter::GetSupplyStatus() noexcept {
    try {
        std::lock_guard<std::mutex> lock(supply_mutex_);

        // 可选：从硬件读取实际 DO 状态验证
        // short value = 0;
        // int result = MC_GetExtDoBit(cardIndex_, 0, &value);
        // if (result == 0) {
        //     supply_status_.state = (value == 1) ? SupplyValveState::Open : SupplyValveState::Closed;
        // }

        return Result<SupplyValveStatusDetail>::Success(supply_status_);
    }
    catch (const std::exception& e) {
        return Result<SupplyValveStatusDetail>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, std::string("Exception: ") + e.what()));
    }
}


} // namespace Adapters
} // namespace Infrastructure
} // namespace Siligen



