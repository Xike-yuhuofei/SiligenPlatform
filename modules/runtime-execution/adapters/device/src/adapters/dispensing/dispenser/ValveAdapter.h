#pragma once

#include "domain/dispensing/ports/IValvePort.h"
#include "process_planning/contracts/configuration/ValveConfig.h"
#include "domain/dispensing/value-objects/DispenseCompensationProfile.h"
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
    class ValveAdapter : public Domain::Dispensing::Ports::IValvePort {
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

        // IValvePort 供胶阀接口实现
        Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveState> OpenSupply() noexcept override;

        Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveState> CloseSupply() noexcept override;

        Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveStatusDetail> GetSupplyStatus() noexcept override;

       private:
        /**
         * @brief 调用 MultiCard CMP 单次脉冲 API
         * @param channel CMP通道号
         * @param durationMs 脉冲持续时间（毫秒）
         * @return 调用结果（0=成功）
         */
        int CallMC_CmpPulseOnce(int16 channel, uint32 durationMs);

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

        /**
         * @brief 重置点胶阀CMP硬件状态（停止触发并拉低输出电平）
         * @param reason 调用原因（用于日志）
         * @param strict true=失败返回错误码，false=失败仅记录日志
         * @return 调用结果（0=成功）
         */
        int ResetDispenserHardwareState(const char* reason, bool strict) noexcept;

        /**
         * @brief 调用MultiCard MC_CmpBufData 加载位置触发缓冲区
         *
         * 注意：CMP 输出通道需要在调用前通过 MC_CmpBufSetChannel 设置。
         * 此函数的第一个参数是位置比较源轴号（SDK 1-based），不是输出通道。
         *
         * @param compare_source_axis 位置比较源轴号（1-16），用于 MC_CmpBufData 的 nCmpEncodeNum
         * @param positions 位置数组（脉冲单位）
         * @param pulse_width_ms 脉冲宽度（毫秒）
         * @param start_level 起始电平（0=低，1=高）
         * @return 调用结果（0=成功）
         */
        int CallMC_CmpBufData(short compare_source_axis,
                              const std::vector<long>& positions,
                              uint32 pulse_width_ms,
                              short start_level);

        int CallMC_CmpBufDataBuffers(short compare_source_axis,
                                     const std::vector<long>* buffer1,
                                     const std::vector<long>* buffer2,
                                     uint32 pulse_width_ms,
                                     short start_level);

        /**
         * @brief 调用MultiCard MC_EncOff 关闭编码器反馈
         * @param axis 轴号（1-16）
         * @return 调用结果（0=成功）
         */
        int CallMC_EncOff(short axis);

        /**
         * @brief 更新点胶阀状态（计算进度）
         */
        void UpdateDispenserProgress();

        /**
         * @brief 自动刷新计时模式点胶阀状态（完成则切换为空闲）
         */
        void RefreshTimedDispenserStateIfCompleted() noexcept;

        /**
         * @brief 创建错误信息
         * @param errorCode MultiCard错误码
         * @param operation 操作名称
         * @return 错误字符串
         */
        std::string CreateErrorMessage(int errorCode, const std::string& operation) const;

        void StopCmpBufferStreaming() noexcept;
        void StartCmpBufferStreamingThread() noexcept;
        void CmpBufferStreamingLoop() noexcept;
        bool LoadCmpBufferChunk(bool load_buf1) noexcept;
        void TimedDispenserLoop() noexcept;
        void StopTimedDispenserThread() noexcept;
        void JoinTimedDispenserThreadIfFinished() noexcept;

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

        // CMP 位置触发流式加载状态
        std::atomic<bool> cmp_streaming_active_{false};
        std::atomic<bool> cmp_stream_stop_{false};
        std::thread cmp_stream_thread_;
        std::mutex cmp_stream_mutex_;
        std::vector<long> cmp_stream_positions_;
        size_t cmp_stream_next_chunk_ = 0;
        size_t cmp_stream_total_chunks_ = 0;
        short cmp_stream_compare_source_axis_ = 0;
        uint32 cmp_stream_pulse_width_ms_ = 0;
        short cmp_stream_start_level_ = 0;

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









