// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "ValveAdapter"

#include "ValveAdapter.h"
#include "shared/interfaces/ILoggingService.h"

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

using namespace Domain::Dispensing::Ports;
using namespace Shared::Types;

// ============================================================
// 构造与析构
// ============================================================

// CLAUDE_SUPPRESS: FORBIDDEN_DOMAIN_EXCEPTIONS
// Reason: 适配器构造函数参数验证失败时需要抛出异常，符合C++标准库惯用法。
//         Infrastructure层适配器在初始化时必须确保依赖项有效性，无法继续运行时应快速失败。
// Approved-By: Claude AI Auto-Fix
// Date: 2026-01-08
ValveAdapter::ValveAdapter(std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> hardware,
                           const Domain::Configuration::Ports::ValveSupplyConfig& supply_config,
                           const Shared::Types::DispenserValveConfig& dispenser_config,
                           const Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile& compensation_profile)
    : hardware_(hardware)
    , dispenser_elapsed_before_pause_(0)
    , supply_config_(supply_config)
    , dispenser_config_(dispenser_config)
    , compensation_profile_(compensation_profile)
{
    if (!hardware_) {
        throw std::invalid_argument("ValveAdapter: hardware cannot be null");
    }

    // 验证配置
    if (!supply_config_.IsValid()) {
        throw std::invalid_argument("ValveAdapter: invalid supply config (do_card_index="
                                   + std::to_string(supply_config_.do_card_index)
                                   + ", do_bit_index="
                                   + std::to_string(supply_config_.do_bit_index) + ")");
    }

    // 初始化点胶阀状态
    dispenser_state_.status = DispenserValveStatus::Idle;
    dispenser_state_.completedCount = 0;
    dispenser_state_.totalCount = 0;
    dispenser_state_.remainingCount = 0;
    dispenser_state_.progress = 0.0f;

    // 初始化供胶阀状态
    supply_status_.state = SupplyValveState::Closed;

    SILIGEN_LOG_INFO(std::string("Initialized with configs"));
    SILIGEN_LOG_INFO("Supply: card=" + std::to_string(supply_config_.do_card_index) +
                     ", bit=" + std::to_string(supply_config_.do_bit_index) + " (Y" +
                     std::to_string(supply_config_.do_bit_index) + ")");
    SILIGEN_LOG_INFO("Dispenser: CMP channel=" + std::to_string(dispenser_config_.cmp_channel) +
                     ", pulse_type=" + std::to_string(dispenser_config_.pulse_type));
}

ValveAdapter::~ValveAdapter() {
    try {
        StopProfileCompareWorker();
        StopDispenserInternal("~ValveAdapter");
        CloseSupplyInternal("~ValveAdapter");
    } catch (...) {
        // 析构函数中捕获所有异常
    }
}


} // namespace Adapters
} // namespace Infrastructure
} // namespace Siligen



