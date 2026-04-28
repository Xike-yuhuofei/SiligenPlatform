#pragma once

#include "runtime_execution/contracts/dispensing/DispenseCompensationProfile.h"
#include "runtime_execution/contracts/dispensing/IProfileComparePort.h"
#include "runtime_execution/contracts/dispensing/IValvePort.h"
#include "process_planning/contracts/configuration/ValveConfig.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "shared/types/Error.h"
#include "shared/types/Types.h"
#include "shared/types/ConfigTypes.h"
#include "siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h"
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

    namespace Siligen {
    namespace Infrastructure {
    namespace Adapters {

    /**
     * @brief 阀门控制适配器
     *
     * 实现IValvePort接口，封装MultiCard硬件阀门控制逻辑
     * 作为基础设施层的适配器，负责将领域接口转换为具体的硬件操作
     */
    class ValveAdapter : public Domain::Dispensing::Ports::IValvePort,
                         public Domain::Dispensing::Ports::IProfileComparePort,
                         public Siligen::Device::Contracts::Ports::DispenserDevicePort {
       public:
        /**
         * @brief 构造函数
         * @param hardware IMultiCardWrapper硬件接口对象
         * @param supply_config 供胶阀配置
         * @param dispenser_config 点胶阀配置
         */
        ValveAdapter(std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> hardware,
                     const Domain::Configuration::Ports::ValveSupplyConfig& supply_config,
                     const Shared::Types::DispenserValveConfig& dispenser_config,
                     const Domain::Dispensing::ValueObjects::DispenseCompensationProfile& compensation_profile);

        /**
         * @brief 析构函数
         */
        ~ValveAdapter() override;

        // 禁止拷贝和移动
        ValveAdapter(const ValveAdapter&) = delete;
        ValveAdapter& operator=(const ValveAdapter&) = delete;
        ValveAdapter(ValveAdapter&&) = delete;
        ValveAdapter& operator=(ValveAdapter&&) = delete;

        // IValvePort 点胶阀接口实现
        Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> StartDispenser(
            const Domain::Dispensing::Ports::DispenserValveParams& params) noexcept override;

        Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> OpenDispenser() noexcept override;

        Shared::Types::Result<void> CloseDispenser() noexcept override;

        Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> StartPositionTriggeredDispenser(
            const Domain::Dispensing::Ports::PositionTriggeredDispenserParams& params) noexcept override;

        Shared::Types::Result<void> StopDispenser() noexcept override;

        Shared::Types::Result<void> PauseDispenser() noexcept override;

        Shared::Types::Result<void> ResumeDispenser() noexcept override;

        Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> GetDispenserStatus() noexcept override;

        Shared::Types::Result<void> ArmProfileCompare(
            const Domain::Dispensing::Ports::ProfileCompareArmRequest& request) noexcept override;

        Shared::Types::Result<void> DisarmProfileCompare() noexcept override;

        Shared::Types::Result<Domain::Dispensing::Ports::ProfileCompareStatus> GetProfileCompareStatus() noexcept override;

        // IValvePort 供胶阀接口实现
        Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveState> OpenSupply() noexcept override;

        Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveState> CloseSupply() noexcept override;

        Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveStatusDetail> GetSupplyStatus() noexcept override;

        Siligen::SharedKernel::VoidResult Execute(
            const Siligen::Device::Contracts::Commands::DispenserCommand& command) override;

        Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DispenserState> ReadState() const override;

        Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DispenserCapability>
        DescribeCapability() const override;

       private:
        /**
         * @brief 调用 MultiCard CMP 单次脉冲 API
         * @param channel CMP通道号
         * @param durationMs 脉冲持续时间（毫秒）
         * @return 调用结果（0=成功）
         */
        int CallMC_CmpPulseOnce(int16 channel, uint32 durationMs);
        int CallMC_CmpPulseOnce(int16 channel, uint32 durationMs, short startLevel);
        int CallMC_CmpPulseOnceMicroseconds(int16 channel, uint32 durationUs, short startLevel);

        /**
         * @brief 调用MultiCard CMP电平输出API（连续开阀/关阀）
         * @param high true=高电平，false=低电平
         * @return 调用结果（0=成功）
         */
        int CallMC_SetCmpLevel(bool high);

        /**
         * @brief 调用MultiCard 扩展DO设置API
         * @param value 值（1=高电平，0=低电平）
         * @return 调用结果（0=成功）
         */
        int CallMC_SetExtDoBit(int16 value);

        /**
         * @brief 构建CMP通道掩码（包含默认通道与配置通道）
         * @return 通道掩码
         */
        unsigned short BuildCmpChannelMask() const noexcept;

        /**
         * @brief 调用MultiCard 停止CMP触发API（掩码方式）
         * @param channelMask CMP通道掩码
         * @return 调用结果（0=成功）
         */
        int CallMC_StopCmpOutputs(unsigned short channelMask);

        short ResolvePrimaryCompareSourceAxis(
            const Domain::Dispensing::Ports::ProfileCompareArmRequest& request) const noexcept;
        void ResetProfileCompareTrackingState() noexcept;
        void StopProfileCompareWorker() noexcept;
        void ProfileCompareWorkerLoop() noexcept;
        bool UseAbsoluteProfileComparePositioning() const noexcept;
        static std::string BuildProfileCompareContext(
            const Domain::Dispensing::Ports::ProfileCompareArmRequest& request,
            short compare_source_axis,
            short cmp_channel,
            long base_profile_position_pulse,
            std::size_t future_compare_count,
            short abs_position_flag,
            short timer_flag,
            short sdk_pulse_type,
            const std::vector<long>* compare_positions);
        Shared::Types::Result<void> ValidateProfileCompareRequestForHardware(
            const Domain::Dispensing::Ports::ProfileCompareArmRequest& request,
            short compare_source_axis,
            short cmp_channel,
            short abs_position_flag,
            short timer_flag,
            short sdk_pulse_type,
            long base_profile_position_pulse,
            const std::vector<long>& compare_positions) const noexcept;
        void UpdateProfileCompareStatusFromHardwareSnapshot(unsigned short status,
                                                           unsigned short remain_data_1,
                                                           unsigned short remain_data_2,
                                                           unsigned short remain_space_1,
                                                           unsigned short remain_space_2) noexcept;

        /**
         * @brief 重置点胶阀CMP硬件状态（停止触发并拉低输出电平）
         * @param reason 调用原因（用于日志）
         * @param strict true=失败返回错误码，false=失败仅记录日志
         * @return 调用结果（0=成功）
         */
        int ResetDispenserHardwareState(const char* reason, bool strict) noexcept;

        /**
         * @brief 更新点胶阀状态（计算进度）
         */
        void UpdateDispenserProgress();

        /**
         * @brief 自动刷新计时模式点胶阀状态（完成则切换为空闲）
         */
        void RefreshTimedDispenserStateIfCompleted() noexcept;

        void StopPathTriggeredDispenserThread() noexcept;
        void JoinPathTriggeredDispenserThreadIfFinished() noexcept;
        Shared::Types::Result<void> StopDispenserInternal(const char* operation) noexcept;
        void PathTriggeredDispenserLoop() noexcept;
        bool ReadCoordinateSystemPositionPulse(short coordinate_system,
                                               long& x_position_pulse,
                                               long& y_position_pulse) noexcept;

        /**
         * @brief 创建错误信息
         * @param errorCode MultiCard错误码
         * @param operation 操作名称
         * @return 错误字符串
         */
        static std::string CreateErrorMessage(int errorCode, const std::string& operation);

        void TimedDispenserLoop() noexcept;
        void StopTimedDispenserThread() noexcept;
        void JoinTimedDispenserThreadIfFinished() noexcept;
        Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveState> CloseSupplyInternal(
            const char* operation) noexcept;

       private:
        enum class DispenserRunMode {
            None,
            Timed,
            Continuous,
            PositionTriggered
        };

        // 硬件接口
        std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper>
            hardware_;  ///< IMultiCardWrapper硬件接口对象

        // 点胶阀状态
        mutable std::mutex dispenser_mutex_;                          ///< 点胶阀状态互斥锁
        Domain::Dispensing::Ports::DispenserValveState dispenser_state_;          ///< 点胶阀当前状态
        Domain::Dispensing::Ports::DispenserValveParams dispenser_params_;        ///< 点胶阀参数（用于恢复）
        std::chrono::steady_clock::time_point dispenser_start_time_;  ///< 点胶阀启动时��点
        std::chrono::steady_clock::time_point dispenser_pause_time_;  ///< 点胶阀暂停时间点
        uint32 dispenser_elapsed_before_pause_ = 0;                   ///< 暂停前已运行时间（毫秒）
        bool dispenser_continuous_ = false;                           ///< 是否处于连续开阀模式
        DispenserRunMode dispenser_run_mode_ = DispenserRunMode::None;
        std::condition_variable timed_dispenser_cv_;
        std::thread timed_dispenser_thread_;
        bool timed_dispenser_active_ = false;
        bool timed_dispenser_stop_requested_ = false;
        bool timed_dispenser_pause_requested_ = false;
        std::string last_execution_id_;

        std::condition_variable path_trigger_cv_;
        std::thread path_trigger_thread_;
        bool path_trigger_active_ = false;
        bool path_trigger_stop_requested_ = false;
        bool path_trigger_pause_requested_ = false;
        short path_trigger_coordinate_system_ = 1;
        short path_trigger_start_level_ = 0;
        uint32 path_trigger_pulse_width_ms_ = 0;
        long path_trigger_position_tolerance_pulse_ = 0;
        std::vector<Domain::Dispensing::Ports::PathTriggerEvent> path_trigger_events_;
        size_t path_trigger_next_index_ = 0;

        struct ProfileCompareSession {
            bool active = false;
            bool stop_requested = false;
            short compare_source_axis = 0;
            short cmp_channel = 0;
            short abs_position_flag = 0;
            short timer_flag = 0;
            short sdk_pulse_type = 0;
            short start_level = 0;
            short pulse_width_us = 0;
            std::vector<long> compare_positions_pulse;
            std::size_t submitted_future_compare_count = 0U;
            unsigned short last_status = 0;
            unsigned short last_remain_data_1 = 0;
            unsigned short last_remain_data_2 = 0;
            unsigned short last_remain_space_1 = 0;
            unsigned short last_remain_space_2 = 0;
            std::string compare_context;
        };

        Domain::Dispensing::Ports::ProfileCompareStatus profile_compare_status_;
        Shared::Types::uint32 profile_compare_start_boundary_trigger_count_ = 0;
        std::thread profile_compare_worker_thread_;
        bool profile_compare_worker_active_ = false;
        ProfileCompareSession profile_compare_session_;

        // 供胶阀状态
        mutable std::mutex supply_mutex_;                       ///< 供胶阀状态互斥锁
        Domain::Dispensing::Ports::SupplyValveStatusDetail supply_status_;  ///< 供胶阀详细状态
        Domain::Configuration::Ports::ValveSupplyConfig supply_config_;        ///< 供胶阀配置
        Shared::Types::DispenserValveConfig dispenser_config_;     ///< 点胶阀配置
        Domain::Dispensing::ValueObjects::DispenseCompensationProfile compensation_profile_;
    };

    }  // namespace Adapters
    }  // namespace Infrastructure
}  // namespace Siligen
