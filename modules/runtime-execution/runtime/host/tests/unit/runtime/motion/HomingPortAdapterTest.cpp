#include "siligen/device/adapters/motion/HomingPortAdapter.h"

#include "domain/configuration/ports/IConfigurationPort.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#include "shared/types/HardwareConfiguration.h"

#include <gtest/gtest.h>

#include <array>
#include <memory>

namespace {

using HomingPortAdapter = Siligen::Infrastructure::Adapters::Motion::HomingPortAdapter;
using HomingConfig = Siligen::Domain::Configuration::Ports::HomingConfig;
using MockMultiCard = Siligen::Infrastructure::Hardware::MockMultiCard;
using MockMultiCardWrapper = Siligen::Infrastructure::Hardware::MockMultiCardWrapper;
using HardwareConfiguration = Siligen::Shared::Types::HardwareConfiguration;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using HomingState = Siligen::Domain::Motion::Ports::HomingState;

class HomeReleaseOnJogWrapper final : public MockMultiCardWrapper {
   public:
    explicit HomeReleaseOnJogWrapper(std::shared_ptr<MockMultiCard> mock_card)
        : MockMultiCardWrapper(std::move(mock_card)) {}

    int MC_GetSts(short axis, long* status, short from_axis = 1, unsigned long* clock = nullptr) noexcept override {
        if (axis == 1) {
            if (status == nullptr) {
                return -1;
            }
            *status = axis_status_;
            if (clock != nullptr) {
                *clock = 0;
            }
            return 0;
        }
        return MockMultiCardWrapper::MC_GetSts(axis, status, from_axis, clock);
    }

    int MC_GetDiRaw(short card_index, long* value) noexcept override {
        if (card_index == MC_HOME) {
            if (value == nullptr) {
                return -1;
            }
            *value = home_raw_;
            return 0;
        }
        return MockMultiCardWrapper::MC_GetDiRaw(card_index, value);
    }

    int MC_GetPos(short axis, long* position) noexcept override {
        if (axis == 1) {
            if (position == nullptr) {
                return -1;
            }
            *position = position_;
            return 0;
        }
        return MockMultiCardWrapper::MC_GetPos(axis, position);
    }

    int MC_PrfTrap(short axis) noexcept override {
        if (axis == 1) {
            prf_trap_called_ = true;
            return 0;
        }
        return MockMultiCardWrapper::MC_PrfTrap(axis);
    }

    int MC_PrfJog(short axis) noexcept override {
        if (axis == 1) {
            prf_jog_called_ = true;
            jog_mode_active_ = true;
            return 0;
        }
        return MockMultiCardWrapper::MC_PrfJog(axis);
    }

    int MC_SetJogPrm(short axis, void* jogPrm) noexcept override {
        if (axis == 1) {
            set_jog_prm_called_ = true;
            return 0;
        }
        return MockMultiCardWrapper::MC_SetJogPrm(axis, jogPrm);
    }

    int MC_SetVel(short axis, double velocity) noexcept override {
        if (axis == 1) {
            last_velocity_ = velocity;
            return 0;
        }
        return MockMultiCardWrapper::MC_SetVel(axis, velocity);
    }

    int MC_Update(long mask) noexcept override {
        if ((mask & 0x1L) != 0) {
            update_called_ = true;
            if (jog_mode_active_ && last_velocity_ > 0.0) {
                axis_status_ |= AXIS_STATUS_RUNNING;
                axis_status_ &= ~AXIS_STATUS_HOME_SWITCH;
                if (!keep_home_raw_latched_after_jog_) {
                    home_raw_ &= ~0x1L;
                }
                position_ += 1200;
            }
            return 0;
        }
        return MockMultiCardWrapper::MC_Update(mask);
    }

    int MC_Stop(long mask, short mode) noexcept override {
        if ((mask & 0x1L) != 0) {
            stop_called_ = true;
            axis_status_ &= ~AXIS_STATUS_RUNNING;
            return 0;
        }
        return MockMultiCardWrapper::MC_Stop(mask, mode);
    }

    int MC_HomeStart(short axis) noexcept override {
        if (axis == 1) {
            home_start_called_ = true;
            axis_status_ |= AXIS_STATUS_HOME_RUNNING;
            return 0;
        }
        return MockMultiCardWrapper::MC_HomeStart(axis);
    }

    void SetInitialHomeTriggered() {
        axis_status_ = AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SWITCH;
        home_raw_ = 0x1L;
        position_ = -400;
    }

    void SetKeepHomeRawLatchedAfterJog(bool value) {
        keep_home_raw_latched_after_jog_ = value;
    }

    bool HomeReleased() const { return (home_raw_ & 0x1L) == 0; }
    bool prf_jog_called() const { return prf_jog_called_; }
    bool set_jog_prm_called() const { return set_jog_prm_called_; }
    bool update_called() const { return update_called_; }
    bool stop_called() const { return stop_called_; }
    bool home_start_called() const { return home_start_called_; }
    bool prf_trap_called() const { return prf_trap_called_; }

   private:
    long axis_status_ = AXIS_STATUS_ENABLE;
    long home_raw_ = 0;
    long position_ = 0;
    double last_velocity_ = 0.0;
    bool jog_mode_active_ = false;
    bool prf_trap_called_ = false;
    bool prf_jog_called_ = false;
    bool set_jog_prm_called_ = false;
    bool update_called_ = false;
    bool stop_called_ = false;
    bool home_start_called_ = false;
    bool keep_home_raw_latched_after_jog_ = false;
};

class HomeCompletionRequiresStopWrapper final : public MockMultiCardWrapper {
   public:
    explicit HomeCompletionRequiresStopWrapper(std::shared_ptr<MockMultiCard> mock_card)
        : MockMultiCardWrapper(std::move(mock_card)) {}

    int MC_GetSts(short axis, long* status, short from_axis = 1, unsigned long* clock = nullptr) noexcept override {
        if (axis == 1) {
            if (status == nullptr) {
                return -1;
            }
            if (!home_started_) {
                *status = AXIS_STATUS_ENABLE;
            } else if (running_polls_remaining_ > 0) {
                *status = AXIS_STATUS_ENABLE | AXIS_STATUS_RUNNING | AXIS_STATUS_HOME_RUNNING;
                --running_polls_remaining_;
            } else {
                *status = AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SUCESS;
            }
            if (clock != nullptr) {
                *clock = 0;
            }
            return 0;
        }
        return MockMultiCardWrapper::MC_GetSts(axis, status, from_axis, clock);
    }

    int MC_GetAxisPrfVel(short axis, double* vel, short count = 1, unsigned long* clock = nullptr) noexcept override {
        if (axis == 1) {
            (void)count;
            if (vel == nullptr) {
                return -1;
            }
            *vel = (home_started_ && running_polls_remaining_ > 0) ? 2.0 : 0.0;
            if (clock != nullptr) {
                *clock = 0;
            }
            return 0;
        }
        return MockMultiCardWrapper::MC_GetAxisPrfVel(axis, vel, count, clock);
    }

    int MC_HomeStart(short axis) noexcept override {
        if (axis == 1) {
            home_started_ = true;
            running_polls_remaining_ = 4;
            return 0;
        }
        return MockMultiCardWrapper::MC_HomeStart(axis);
    }

    int MC_HomeGetSts(short axis, unsigned short* status) noexcept override {
        if (axis == 1) {
            if (status == nullptr) {
                return -1;
            }
            *status = home_started_ ? 1 : 0;
            return 0;
        }
        return MockMultiCardWrapper::MC_HomeGetSts(axis, status);
    }

   private:
    bool home_started_ = false;
    int running_polls_remaining_ = 0;
};

class SuccessStatusOverridesHomeApiFailureWrapper final : public MockMultiCardWrapper {
   public:
    explicit SuccessStatusOverridesHomeApiFailureWrapper(std::shared_ptr<MockMultiCard> mock_card)
        : MockMultiCardWrapper(std::move(mock_card)) {}

    int MC_GetSts(short axis, long* status, short from_axis = 1, unsigned long* clock = nullptr) noexcept override {
        if (axis == 1) {
            if (status == nullptr) {
                return -1;
            }
            *status = home_started_ ? (AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SUCESS) : AXIS_STATUS_ENABLE;
            if (clock != nullptr) {
                *clock = 0;
            }
            return 0;
        }
        return MockMultiCardWrapper::MC_GetSts(axis, status, from_axis, clock);
    }

    int MC_GetAxisPrfVel(short axis, double* vel, short count = 1, unsigned long* clock = nullptr) noexcept override {
        if (axis == 1) {
            (void)count;
            if (vel == nullptr) {
                return -1;
            }
            *vel = 0.0;
            if (clock != nullptr) {
                *clock = 0;
            }
            return 0;
        }
        return MockMultiCardWrapper::MC_GetAxisPrfVel(axis, vel, count, clock);
    }

    int MC_HomeStart(short axis) noexcept override {
        if (axis == 1) {
            home_started_ = true;
            return 0;
        }
        return MockMultiCardWrapper::MC_HomeStart(axis);
    }

    int MC_HomeGetSts(short axis, unsigned short* status) noexcept override {
        if (axis == 1) {
            if (status == nullptr) {
                return -1;
            }
            *status = home_started_ ? 2 : 0;
            return 0;
        }
        return MockMultiCardWrapper::MC_HomeGetSts(axis, status);
    }

   private:
    bool home_started_ = false;
};

class HomeCompletionRequiresActiveReleaseWrapper final : public MockMultiCardWrapper {
   public:
    explicit HomeCompletionRequiresActiveReleaseWrapper(std::shared_ptr<MockMultiCard> mock_card)
        : MockMultiCardWrapper(std::move(mock_card)) {}

    int MC_GetSts(short axis, long* status, short from_axis = 1, unsigned long* clock = nullptr) noexcept override {
        if (axis == 1) {
            if (status == nullptr) {
                return -1;
            }
            *status = axis_status_;
            if (clock != nullptr) {
                *clock = 0;
            }
            return 0;
        }
        return MockMultiCardWrapper::MC_GetSts(axis, status, from_axis, clock);
    }

    int MC_GetDiRaw(short card_index, long* value) noexcept override {
        if (card_index == MC_HOME) {
            if (value == nullptr) {
                return -1;
            }
            *value = home_raw_;
            return 0;
        }
        return MockMultiCardWrapper::MC_GetDiRaw(card_index, value);
    }

    int MC_GetPos(short axis, long* position) noexcept override {
        if (axis == 1) {
            if (position == nullptr) {
                return -1;
            }
            *position = position_;
            return 0;
        }
        return MockMultiCardWrapper::MC_GetPos(axis, position);
    }

    int MC_PrfJog(short axis) noexcept override {
        if (axis == 1) {
            prf_jog_called_ = true;
            jog_mode_active_ = true;
            return 0;
        }
        return MockMultiCardWrapper::MC_PrfJog(axis);
    }

    int MC_SetJogPrm(short axis, void* jogPrm) noexcept override {
        if (axis == 1) {
            set_jog_prm_called_ = true;
            return 0;
        }
        return MockMultiCardWrapper::MC_SetJogPrm(axis, jogPrm);
    }

    int MC_SetVel(short axis, double velocity) noexcept override {
        if (axis == 1) {
            last_velocity_ = velocity;
            return 0;
        }
        return MockMultiCardWrapper::MC_SetVel(axis, velocity);
    }

    int MC_Update(long mask) noexcept override {
        if ((mask & 0x1L) != 0) {
            update_called_ = true;
            if (jog_mode_active_ && last_velocity_ > 0.0) {
                axis_status_ = AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SUCESS | AXIS_STATUS_RUNNING;
                home_raw_ &= ~0x1L;
                position_ += 1200;
            }
            return 0;
        }
        return MockMultiCardWrapper::MC_Update(mask);
    }

    int MC_Stop(long mask, short mode) noexcept override {
        if ((mask & 0x1L) != 0) {
            stop_called_ = true;
            axis_status_ = AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SUCESS;
            jog_mode_active_ = false;
            return 0;
        }
        return MockMultiCardWrapper::MC_Stop(mask, mode);
    }

    int MC_HomeStart(short axis) noexcept override {
        if (axis == 1) {
            home_started_ = true;
            axis_status_ = AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SUCESS | AXIS_STATUS_HOME_SWITCH;
            home_raw_ = 0x1L;
            return 0;
        }
        return MockMultiCardWrapper::MC_HomeStart(axis);
    }

    int MC_HomeGetSts(short axis, unsigned short* status) noexcept override {
        if (axis == 1) {
            if (status == nullptr) {
                return -1;
            }
            *status = home_started_ ? 1 : 0;
            return 0;
        }
        return MockMultiCardWrapper::MC_HomeGetSts(axis, status);
    }

    int MC_GetAxisPrfVel(short axis, double* vel, short count = 1, unsigned long* clock = nullptr) noexcept override {
        if (axis == 1) {
            (void)count;
            if (vel == nullptr) {
                return -1;
            }
            *vel = jog_mode_active_ ? 2.0 : 0.0;
            if (clock != nullptr) {
                *clock = 0;
            }
            return 0;
        }
        return MockMultiCardWrapper::MC_GetAxisPrfVel(axis, vel, count, clock);
    }

    bool prf_jog_called() const { return prf_jog_called_; }
    bool update_called() const { return update_called_; }
    bool stop_called() const { return stop_called_; }

   private:
    long axis_status_ = AXIS_STATUS_ENABLE | AXIS_STATUS_HOME_SWITCH;
    long home_raw_ = 0x1L;
    long position_ = -400;
    double last_velocity_ = 0.0;
    bool home_started_ = false;
    bool jog_mode_active_ = false;
    bool prf_jog_called_ = false;
    bool set_jog_prm_called_ = false;
    bool update_called_ = false;
    bool stop_called_ = false;
};

TEST(HomingPortAdapterTest, EscapesTriggeredHomeByJoggingAwayBeforeStartingHoming) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<HomeReleaseOnJogWrapper>(mock_card);
    wrapper->SetInitialHomeTriggered();

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;
    hardware_config.soft_limit_negative_mm = 0.0f;
    hardware_config.soft_limit_positive_mm = 480.0f;

    std::array<HomingConfig, 4> homing_configs{};
    homing_configs[0].axis = 0;
    homing_configs[0].mode = 1;
    homing_configs[0].direction = 0;
    homing_configs[0].rapid_velocity = 10.0f;
    homing_configs[0].locate_velocity = 5.0f;
    homing_configs[0].acceleration = 500.0f;
    homing_configs[0].deceleration = 500.0f;
    homing_configs[0].search_distance = 200.0f;
    homing_configs[0].escape_distance = 5.0f;
    homing_configs[0].escape_velocity = 5.0f;
    homing_configs[0].timeout_ms = 1000;
    homing_configs[0].escape_timeout_ms = 1000;
    homing_configs[0].escape_max_attempts = 1;
    homing_configs[0].enable_escape = true;
    homing_configs[0].enable_limit_switch = true;
    homing_configs[0].enable_index = false;
    homing_configs[0].home_backoff_enabled = false;
    homing_configs[0].home_input_source = MC_HOME;
    homing_configs[0].home_input_bit = 0;
    homing_configs[0].home_active_low = false;
    homing_configs[0].home_debounce_ms = 0;

    auto adapter = HomingPortAdapter(wrapper, hardware_config, homing_configs);

    auto result = adapter.HomeAxis(LogicalAxisId::X);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_TRUE(wrapper->prf_jog_called());
    EXPECT_TRUE(wrapper->set_jog_prm_called());
    EXPECT_TRUE(wrapper->update_called());
    EXPECT_TRUE(wrapper->stop_called());
    EXPECT_TRUE(wrapper->home_start_called());
    EXPECT_TRUE(wrapper->HomeReleased());
    EXPECT_FALSE(wrapper->prf_trap_called());
}

TEST(HomingPortAdapterTest, EscapesTriggeredHomeWhenStatusBitReleasesButRawHomeRemainsLatched) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<HomeReleaseOnJogWrapper>(mock_card);
    wrapper->SetInitialHomeTriggered();
    wrapper->SetKeepHomeRawLatchedAfterJog(true);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;
    hardware_config.soft_limit_negative_mm = 0.0f;
    hardware_config.soft_limit_positive_mm = 480.0f;

    std::array<HomingConfig, 4> homing_configs{};
    homing_configs[0].axis = 0;
    homing_configs[0].mode = 1;
    homing_configs[0].direction = 0;
    homing_configs[0].rapid_velocity = 10.0f;
    homing_configs[0].locate_velocity = 5.0f;
    homing_configs[0].acceleration = 500.0f;
    homing_configs[0].deceleration = 500.0f;
    homing_configs[0].search_distance = 200.0f;
    homing_configs[0].escape_distance = 5.0f;
    homing_configs[0].escape_velocity = 5.0f;
    homing_configs[0].timeout_ms = 1000;
    homing_configs[0].escape_timeout_ms = 1000;
    homing_configs[0].escape_max_attempts = 1;
    homing_configs[0].enable_escape = true;
    homing_configs[0].enable_limit_switch = true;
    homing_configs[0].enable_index = false;
    homing_configs[0].home_backoff_enabled = false;
    homing_configs[0].home_input_source = MC_HOME;
    homing_configs[0].home_input_bit = 0;
    homing_configs[0].home_active_low = false;
    homing_configs[0].home_debounce_ms = 0;

    auto adapter = HomingPortAdapter(wrapper, hardware_config, homing_configs);

    auto result = adapter.HomeAxis(LogicalAxisId::X);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_TRUE(wrapper->prf_jog_called());
    EXPECT_TRUE(wrapper->set_jog_prm_called());
    EXPECT_TRUE(wrapper->update_called());
    EXPECT_TRUE(wrapper->stop_called());
    EXPECT_TRUE(wrapper->home_start_called());
    EXPECT_FALSE(wrapper->HomeReleased());
}

TEST(HomingPortAdapterTest, DoesNotReportHomedUntilCompletedSignalStopsAndVelocityDropsToZero) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<HomeCompletionRequiresStopWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;
    hardware_config.soft_limit_negative_mm = 0.0f;
    hardware_config.soft_limit_positive_mm = 480.0f;

    std::array<HomingConfig, 4> homing_configs{};
    homing_configs[0].axis = 0;
    homing_configs[0].mode = 1;
    homing_configs[0].direction = 0;
    homing_configs[0].rapid_velocity = 10.0f;
    homing_configs[0].locate_velocity = 5.0f;
    homing_configs[0].acceleration = 500.0f;
    homing_configs[0].deceleration = 500.0f;
    homing_configs[0].search_distance = 200.0f;
    homing_configs[0].timeout_ms = 1000;
    homing_configs[0].enable_escape = true;
    homing_configs[0].enable_limit_switch = true;
    homing_configs[0].enable_index = false;
    homing_configs[0].home_backoff_enabled = false;
    homing_configs[0].home_input_source = MC_HOME;
    homing_configs[0].home_input_bit = 0;
    homing_configs[0].home_active_low = false;
    homing_configs[0].home_debounce_ms = 0;

    auto adapter = HomingPortAdapter(wrapper, hardware_config, homing_configs);

    auto start_result = adapter.HomeAxis(LogicalAxisId::X);
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().GetMessage();

    auto status1 = adapter.GetHomingStatus(LogicalAxisId::X);
    ASSERT_TRUE(status1.IsSuccess()) << status1.GetError().GetMessage();
    EXPECT_EQ(status1.Value().state, HomingState::HOMING);

    auto status2 = adapter.GetHomingStatus(LogicalAxisId::X);
    ASSERT_TRUE(status2.IsSuccess()) << status2.GetError().GetMessage();
    EXPECT_EQ(status2.Value().state, HomingState::HOMING);

    auto status3 = adapter.GetHomingStatus(LogicalAxisId::X);
    ASSERT_TRUE(status3.IsSuccess()) << status3.GetError().GetMessage();
    EXPECT_EQ(status3.Value().state, HomingState::HOMED);
}

TEST(HomingPortAdapterTest, PrefersHardwareSuccessBitOverHomeApiFailureCode) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<SuccessStatusOverridesHomeApiFailureWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;
    hardware_config.soft_limit_negative_mm = 0.0f;
    hardware_config.soft_limit_positive_mm = 480.0f;

    std::array<HomingConfig, 4> homing_configs{};
    homing_configs[0].axis = 0;
    homing_configs[0].mode = 1;
    homing_configs[0].direction = 0;
    homing_configs[0].rapid_velocity = 10.0f;
    homing_configs[0].locate_velocity = 5.0f;
    homing_configs[0].acceleration = 500.0f;
    homing_configs[0].deceleration = 500.0f;
    homing_configs[0].search_distance = 200.0f;
    homing_configs[0].timeout_ms = 1000;
    homing_configs[0].enable_escape = true;
    homing_configs[0].enable_limit_switch = true;
    homing_configs[0].enable_index = false;
    homing_configs[0].home_backoff_enabled = false;
    homing_configs[0].home_input_source = MC_HOME;
    homing_configs[0].home_input_bit = 0;
    homing_configs[0].home_active_low = false;
    homing_configs[0].home_debounce_ms = 0;

    auto adapter = HomingPortAdapter(wrapper, hardware_config, homing_configs);

    auto start_result = adapter.HomeAxis(LogicalAxisId::X);
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().GetMessage();

    auto wait_result = adapter.WaitForHomingComplete(LogicalAxisId::X, 1000);
    ASSERT_TRUE(wait_result.IsSuccess()) << wait_result.GetError().GetMessage();

    auto status = adapter.GetHomingStatus(LogicalAxisId::X);
    ASSERT_TRUE(status.IsSuccess()) << status.GetError().GetMessage();
    EXPECT_EQ(status.Value().state, HomingState::HOMED);
}

TEST(HomingPortAdapterTest, ActivelyReleasesHomeSwitchAfterHomingCompletionBeforeReportingHomed) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<HomeCompletionRequiresActiveReleaseWrapper>(mock_card);

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;
    hardware_config.max_acceleration_mm_s2 = 500.0f;
    hardware_config.max_deceleration_mm_s2 = 500.0f;
    hardware_config.soft_limit_negative_mm = 0.0f;
    hardware_config.soft_limit_positive_mm = 480.0f;

    std::array<HomingConfig, 4> homing_configs{};
    homing_configs[0].axis = 0;
    homing_configs[0].mode = 1;
    homing_configs[0].direction = 0;
    homing_configs[0].rapid_velocity = 10.0f;
    homing_configs[0].locate_velocity = 5.0f;
    homing_configs[0].acceleration = 500.0f;
    homing_configs[0].deceleration = 500.0f;
    homing_configs[0].search_distance = 200.0f;
    homing_configs[0].escape_distance = 5.0f;
    homing_configs[0].escape_velocity = 5.0f;
    homing_configs[0].timeout_ms = 1000;
    homing_configs[0].escape_timeout_ms = 1000;
    homing_configs[0].escape_max_attempts = 1;
    homing_configs[0].enable_escape = true;
    homing_configs[0].enable_limit_switch = true;
    homing_configs[0].enable_index = false;
    homing_configs[0].home_backoff_enabled = true;
    homing_configs[0].home_input_source = MC_HOME;
    homing_configs[0].home_input_bit = 0;
    homing_configs[0].home_active_low = false;
    homing_configs[0].home_debounce_ms = 0;

    auto adapter = HomingPortAdapter(wrapper, hardware_config, homing_configs);

    auto start_result = adapter.HomeAxis(LogicalAxisId::X);
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().GetMessage();

    auto wait_result = adapter.WaitForHomingComplete(LogicalAxisId::X, 1000);
    ASSERT_TRUE(wait_result.IsSuccess()) << wait_result.GetError().GetMessage();
    EXPECT_TRUE(wrapper->prf_jog_called());
    EXPECT_TRUE(wrapper->update_called());
    EXPECT_TRUE(wrapper->stop_called());

    auto status = adapter.GetHomingStatus(LogicalAxisId::X);
    ASSERT_TRUE(status.IsSuccess()) << status.GetError().GetMessage();
    EXPECT_EQ(status.Value().state, HomingState::HOMED);
}

}  // namespace
