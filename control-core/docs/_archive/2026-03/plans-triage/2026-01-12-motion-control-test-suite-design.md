# Motion Control System Test Suite Design

**Version:** 1.0.0
**Date:** 2026-01-12
**Author:** Tester Agent
**Status:** Design Draft

---

## Table of Contents

1. [Overview](#overview)
2. [Test Architecture](#test-architecture)
3. [Test Base Classes](#test-base-classes)
4. [Helper Utilities](#helper-utilities)
5. [Test Case Catalog](#test-case-catalog)
6. [Test Templates](#test-templates)
7. [Fault Injection Templates](#fault-injection-templates)
8. [Implementation Guidelines](#implementation-guidelines)

---

## Overview

This document provides a comprehensive test suite design for the motion control system, covering:

- **Unit Tests**: Validate business logic using Mocks
- **State Machine Tests**: Verify state transitions using Sim + FakeClock
- **Integration Tests**: Test multi-axis coordination and interactions
- **Fault Injection Tests**: Verify error handling and recovery

### Test Framework Stack

- **Test Framework**: Google Test (gtest)
- **Mocking Framework**: Google Mock (gmock)
- **Assertions**: gtest EXPECT/ASSERT macros
- **Time Simulation**: FakeClock for deterministic timing tests

---

## Test Architecture

### Layer Structure

```
tests/
├── unit/
│   ├── domain/
│   │   ├── services/        # Domain service unit tests
│   │   ├── motion/          # TrajectoryExecutor, Interpolators
│   │   └── triggering/      # CMP, Position Trigger tests
│   ├── application/
│   │   └── usecases/        # Use case tests with Mocks
│   ├── integration/
│   │   ├── motion/          # Multi-axis coordination
│   │   └── io/              # IO联动测试
│   └── mocks/               # Mock implementations
├── fixtures/
│   ├── MotionTestFixtures.h # Base test fixtures
│   ├── SimulatedAxis.h      # Axis simulation
│   └── FakeClock.h          # Time control
└── helpers/
    ├── TestAssertions.h     # Custom assertions
    └── TestBuilders.h       # Test data builders
```

### Dependency Diagram

```
                    Test Suite
                        |
        +---------------+---------------+
        |               |               |
    Unit Tests    State Machine    Integration
        |               |               |
    [Mocks]        [Sim + Clock]   [Fake Hardware]
        |               |               |
        +-------+-------+-------+-------+
                |
        Domain/Application Layer
```

---

## Test Base Classes

### 1. MotionTestBase Fixture

**Location:** `tests/fixtures/MotionTestFixtures.h`

```cpp
#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include "domain/<subdomain>/ports/IPositionControlPort.h"
#include "domain/<subdomain>/ports/IHomingPort.h"
#include "domain/<subdomain>/ports/IHardwareControllerPort.h"
#include "shared/types/Result.h"

namespace Siligen::Test {

using namespace Shared::Types;
using namespace Domain::Ports;

/**
 * @brief Base fixture for all motion control tests
 *
 * Provides common setup, teardown, and helper methods
 */
class MotionTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test environment
        SetupMockPorts();
        SetDefaultParameters();
    }

    void TearDown() override {
        // Cleanup
        CleanupMockPorts();
    }

    virtual void SetupMockPorts() = 0;
    virtual void CleanupMockPorts() = 0;

    void SetDefaultParameters() {
        default_velocity_ = 100.0f;
        default_acceleration_ = 500.0f;
        default_timeout_ms_ = 5000;
    }

    // ============ Common Parameters ============

    float32 default_velocity_;
    float32 default_acceleration_;
    int32_t default_timeout_ms_;
};

} // namespace Siligen::Test
```

### 2. SimulatedAxis Fixture

**Location:** `tests/fixtures/SimulatedAxis.h`

```cpp
#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <cmath>
#include "shared/types/Point.h"
#include "shared/types/Types.h"

namespace Siligen::Test {

using namespace Shared::Types;

/**
 * @brief Simulated axis for state machine testing
 *
 * Models axis physics and state transitions without real hardware
 */
class SimulatedAxis {
public:
    enum class State {
        Disabled,
        Enabled,
        Homing,
        Homed,
        Moving,
        InPosition,
        Error,
        EmergencyStopped
    };

    struct Config {
        float32 max_velocity = 500.0f;
        float32 max_acceleration = 1000.0f;
        float32 soft_limit_positive = 500.0f;
        float32 soft_limit_negative = -500.0f;
        float32 in_position_window = 0.05f;
    };

    SimulatedAxis(int axis_id, const Config& config)
        : axis_id_(axis_id)
        , config_(config)
        , state_(State::Disabled)
        , position_(0.0f)
        , velocity_(0.0f)
        , target_position_(0.0f)
    {}

    // ============ State Control ============

    void Enable() noexcept {
        if (state_ == State::Disabled) {
            state_ = State::Enabled;
        }
    }

    void Disable() noexcept {
        state_ = State::Disabled;
        velocity_ = 0.0f;
    }

    void Home() noexcept {
        if (state_ == State::Enabled) {
            state_ = State::Homing;
            target_position_ = 0.0f;
        }
    }

    void MoveTo(float32 target_pos, float32 velocity) noexcept {
        if (state_ == State::Homed || state_ == State::InPosition) {
            state_ = State::Moving;
            target_position_ = target_pos;
            target_velocity_ = velocity;
        }
    }

    void EmergencyStop() noexcept {
        state_ = State::EmergencyStopped;
        velocity_ = 0.0f;
    }

    // ============ Simulation Step ============

    void Tick(float32 delta_time_seconds) noexcept {
        if (state_ != State::Moving && state_ != State::Homing) {
            return;
        }

        // Simulate acceleration
        float32 accel = config_.max_acceleration;
        float32 velocity_delta = accel * delta_time_seconds;

        if (velocity_ < target_velocity_) {
            velocity_ = std::min(velocity_ + velocity_delta, target_velocity_);
        } else if (velocity_ > target_velocity_) {
            velocity_ = std::max(velocity_ - velocity_delta, target_velocity_);
        }

        // Update position
        float32 position_delta = velocity_ * delta_time_seconds;

        if (state_ == State::Homing) {
            // Move towards zero
            if (position_ > 0) {
                position_ = std::max(0.0f, position_ - position_delta);
            } else {
                position_ = std::min(0.0f, position_ + position_delta);
            }

            // Check if homing complete
            if (std::abs(position_) < 0.01f) {
                position_ = 0.0f;
                state_ = State::Homed;
                velocity_ = 0.0f;
            }
        } else if (state_ == State::Moving) {
            // Move towards target
            float32 direction = (target_position_ > position_) ? 1.0f : -1.0f;
            position_ += direction * position_delta;

            // Check if in position
            if (std::abs(position_ - target_position_) < config_.in_position_window) {
                position_ = target_position_;
                state_ = State::InPosition;
                velocity_ = 0.0f;
            }

            // Check soft limits
            if (position_ > config_.soft_limit_positive ||
                position_ < config_.soft_limit_negative) {
                state_ = State::Error;
                velocity_ = 0.0f;
            }
        }
    }

    // ============ State Query ============

    State GetState() const noexcept { return state_; }
    float32 GetPosition() const noexcept { return position_; }
    float32 GetVelocity() const noexcept { return velocity_; }
    bool IsInPosition() const noexcept { return state_ == State::InPosition; }
    bool IsHomed() const noexcept { return state_ == State::Homed; }
    bool IsError() const noexcept { return state_ == State::Error; }

private:
    int axis_id_;
    Config config_;
    State state_;
    float32 position_;
    float32 velocity_;
    float32 target_position_;
    float32 target_velocity_;
};

} // namespace Siligen::Test
```

### 3. FakeClock Fixture

**Location:** `tests/fixtures/FakeClock.h`

```cpp
#pragma once

#include <chrono>
#include <functional>
#include <vector>

namespace Siligen::Test {

/**
 * @brief Controllable clock for deterministic timing tests
 *
 * Replaces system clock with simulated time
 */
class FakeClock {
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::steady_clock::duration;
    using Callback = std::function<void()>;

    FakeClock()
        : current_time_(std::chrono::steady_clock::time_point{})
    {}

    // ============ Time Control ============

    /**
     * @brief Advance time by specified duration
     */
    template<typename Rep, typename Period>
    void tick(std::chrono::duration<Rep, Period> delta) {
        auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(delta);
        current_time_ += delta;

        // Trigger scheduled callbacks
        ProcessCallbacks(delta_ms);
    }

    /**
     * @brief Get current simulated time
     */
    TimePoint now() const noexcept {
        return current_time_;
    }

    /**
     * @brief Schedule callback at specific delay
     */
    template<typename Rep, typename Period>
    void ScheduleAfter(std::chrono::duration<Rep, Period> delay, Callback callback) {
        auto delay_ms = std::chrono::duration_cast<std::chrono::milliseconds>(delay);
        scheduled_callbacks_.push_back({
            current_time_ + delay_ms,
            callback
        });
    }

    // ============ Static Access for Dependency Injection ============

    static FakeClock& Instance() {
        static FakeClock instance;
        return instance;
    }

    static TimePoint Now() {
        return Instance().now();
    }

private:
    struct ScheduledCallback {
        TimePoint execute_time;
        Callback callback;
    };

    void ProcessCallbacks(std::chrono::milliseconds delta) {
        for (auto& cb : scheduled_callbacks_) {
            if (current_time_ >= cb.execute_time) {
                cb.callback();
            }
        }

        // Remove executed callbacks
        scheduled_callbacks_.erase(
            std::remove_if(scheduled_callbacks_.begin(), scheduled_callbacks_.end(),
                [this](const ScheduledCallback& cb) {
                    return current_time_ >= cb.execute_time;
                }),
            scheduled_callbacks_.end()
        );
    }

    TimePoint current_time_;
    std::vector<ScheduledCallback> scheduled_callbacks_;
};

} // namespace Siligen::Test
```

---

## Helper Utilities

### 1. Test Assertions

**Location:** `tests/helpers/TestAssertions.h`

```cpp
#pragma once

#include <gtest/gtest.h>
#include "shared/types/Result.h"
#include "fixtures/SimulatedAxis.h"

namespace Siligen::Test {

// ============ Result<T> Assertions ============

/**
 * @brief Assert Result is success
 */
template<typename T>
void AssertResultSuccess(const Shared::Types::Result<T>& result,
                         const std::string& msg = "") {
    ASSERT_TRUE(result.IsSuccess())
        << msg << " - Expected Success but got Error: "
        << result.GetError().GetMessage();
}

/**
 * @brief Assert Result is error with specific code
 */
template<typename T>
void AssertResultError(const Shared::Types::Result<T>& result,
                       Shared::Types::ErrorCode expected_code,
                       const std::string& msg = "") {
    ASSERT_TRUE(result.IsError())
        << msg << " - Expected Error but got Success";

    EXPECT_EQ(result.GetError().GetCode(), expected_code)
        << msg << " - Error code mismatch";
}

/**
 * @brief Expect Result is success
 */
template<typename T>
void ExpectResultSuccess(const Shared::Types::Result<T>& result,
                         const std::string& msg = "") {
    EXPECT_TRUE(result.IsSuccess())
        << msg << " - Expected Success but got Error: "
        << result.GetError().GetMessage();
}

// ============ State Assertions ============

/**
 * @brief Assert SimulatedAxis is in specific state
 */
void AssertAxisState(SimulatedAxis& axis,
                     SimulatedAxis::State expected_state,
                     const std::string& msg = "") {
    EXPECT_EQ(axis.GetState(), expected_state)
        << msg << " - Expected state "
        << static_cast<int>(expected_state)
        << " but got "
        << static_cast<int>(axis.GetState());
}

/**
 * @brief Assert axis is in position within tolerance
 */
void AssertInPosition(SimulatedAxis& axis,
                     float32 expected_position,
                     float32 tolerance = 0.01f,
                     const std::string& msg = "") {
    EXPECT_TRUE(axis.IsInPosition())
        << msg << " - Axis not in position";

    EXPECT_NEAR(axis.GetPosition(), expected_position, tolerance)
        << msg << " - Position mismatch";
}

// ============ Wait Helpers ============

/**
 * @brief Wait for axis to reach state (with timeout)
 *
 * @returns true if state reached, false if timeout
 */
bool WaitForState(SimulatedAxis& axis,
                  SimulatedAxis::State target_state,
                  std::chrono::milliseconds timeout,
                  FakeClock& clock) {
    auto start = clock.now();
    auto check_interval = std::chrono::milliseconds(10);

    while (clock.now() - start < timeout) {
        if (axis.GetState() == target_state) {
            return true;
        }
        clock.tick(check_interval);
        axis.Tick(std::chrono::duration<float>(check_interval).count());
    }

    return false;
}

/**
 * @brief Wait for axis to be in position
 */
bool WaitForInPosition(SimulatedAxis& axis,
                       std::chrono::milliseconds timeout,
                       FakeClock& clock) {
    return WaitForState(axis, SimulatedAxis::State::InPosition, timeout, clock);
}

/**
 * @brief Wait for axis to complete homing
 */
bool WaitForHomed(SimulatedAxis& axis,
                  std::chrono::milliseconds timeout,
                  FakeClock& clock) {
    return WaitForState(axis, SimulatedAxis::State::Homed, timeout, clock);
}

} // namespace Siligen::Test
```

### 2. Test Data Builders

**Location:** `tests/helpers/TestBuilders.h`

```cpp
#pragma once

#include "domain/<subdomain>/ports/IHomingPort.h"
#include "domain/<subdomain>/ports/IPositionControlPort.h"
#include "shared/types/Point.h"

namespace Siligen::Test {

using namespace Domain::Ports;

/**
 * @brief Builder for HomingParameters
 */
class HomingParametersBuilder {
public:
    HomingParametersBuilder() = default;

    HomingParametersBuilder& Axis(short axis) {
        params_.axis = axis;
        return *this;
    }

    HomingParametersBuilder& Mode(int mode) {
        params_.mode = mode;
        return *this;
    }

    HomingParametersBuilder& Velocity(float32 vel) {
        params_.velocity = vel;
        return *this;
    }

    HomingParametersBuilder& SearchVelocity(float32 vel) {
        params_.search_velocity = vel;
        return *this;
    }

    HomingParametersBuilder& Timeout(int32_t ms) {
        params_.timeout_ms = ms;
        return *this;
    }

    HomingParameters Build() const {
        return params_;
    }

private:
    HomingParameters params_;
};

/**
 * @brief Builder for motion parameters
 */
class MotionParametersBuilder {
public:
    struct MotionParams {
        short axis;
        float32 target_position;
        float32 velocity;
        float32 acceleration;
        int32_t timeout_ms;
    };

    MotionParametersBuilder()
        : params_{0, 100.0f, 100.0f, 500.0f, 5000}
    {}

    MotionParametersBuilder& Axis(short axis) {
        params_.axis = axis;
        return *this;
    }

    MotionParametersBuilder& Target(float32 pos) {
        params_.target_position = pos;
        return *this;
    }

    MotionParametersBuilder& Velocity(float32 vel) {
        params_.velocity = vel;
        return *this;
    }

    MotionParametersBuilder& Acceleration(float32 accel) {
        params_.acceleration = accel;
        return *this;
    }

    MotionParametersBuilder& Timeout(int32_t ms) {
        params_.timeout_ms = ms;
        return *this;
    }

    MotionParams Build() const {
        return params_;
    }

private:
    MotionParams params_;
};

/**
 * @brief Builder for trajectory points
 */
class TrajectoryPointBuilder {
public:
    TrajectoryPointBuilder() = default;

    TrajectoryPointBuilder& Position(float32 x, float32 y) {
        point_.position = Point2D(x, y);
        return *this;
    }

    TrajectoryPointBuilder& Velocity(float32 vel) {
        point_.velocity = vel;
        return *this;
    }

    TrajectoryPointBuilder& Acceleration(float32 accel) {
        point_.acceleration = accel;
        return *this;
    }

    TrajectoryPointBuilder& SegmentType(SegmentType type) {
        point_.segment_type = type;
        return *this;
    }

    Shared::Types::TrajectoryPoint Build() const {
        return point_;
    }

private:
    Shared::Types::TrajectoryPoint point_;
};

} // namespace Siligen::Test
```

---

## Test Case Catalog

### Category 1: Unit Tests (with Mocks)

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| U001 | HomingController_ValidParams_CallsPort | Verify correct port invocation | P0 |
| U002 | HomingController_InvalidAxis_ReturnsError | Parameter validation | P0 |
| U003 | JogController_StartJog_ValidParams_Succeeds | Jog execution | P0 |
| U004 | JogController_NegativeSpeed_ReturnsError | Speed validation | P1 |
| U005 | MotionService_MoveToPosition_CallsPort | Motion execution | P0 |
| U006 | MotionService_ExceedsSoftLimit_ReturnsError | Boundary checking | P0 |
| U007 | ValveController_Open_ValveStateTransitions | State machine | P1 |
| U008 | CMPService_TriggerAtPosition_CalculatesCorrectly | CMP math | P1 |
| U009 | TrajectoryExecutor_Start_ExecuteAllPoints | Trajectory execution | P0 |
| U010 | Interpolator_Linear_GeneratesCorrectPath | Interpolation accuracy | P1 |

### Category 2: State Machine Tests (with Sim + FakeClock)

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| S001 | HomeSuccess_ShouldTransitionToHomedState | Normal homing flow | P0 |
| S002 | HomeTimeout_ShouldReturnError | Timeout handling | P0 |
| S003 | HomeDuringEmergencyStop_ShouldReturnError | Safety check | P0 |
| S004 | MoveToPosition_ShouldReachTarget | Normal motion | P0 |
| S005 | MoveExceedsSoftLimit_ShouldTriggerAlarm | Soft limit protection | P0 |
| S006 | EmergencyStopDuringMove_ShouldStopImmediately | E-stop behavior | P0 |
| S007 | MultiAxisSync_AllAxesCompleteTogether | Synchronization | P1 |
| S008 | SequentialHoming_AxisByAxis | Sequential mode | P1 |
| S009 | CMPTriggerAtPosition_FiresAtCorrectLocation | CMP trigger | P1 |
| S010 | Interpolation_LinearPath_MatchesExpected | Interpolation | P2 |

### Category 3: Integration Tests

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| I001 | XYPlane_LinearInterpolation_CoordinatedMotion | 2D coordination | P0 |
| I002 | XYZSpace_3DArc_MaintainsGeometry | 3D interpolation | P1 |
| I003 | CMP_MultiAxisTrigger_CorrectTiming | Multi-axis CMP | P1 |
| I004 | Valve_TriggeredDuringMotion_CorrectTiming | IO coordination | P1 |
| I005 | DiagonalMove_XYEqualSpeed_Coordinated | Diagonal motion | P2 |
| I006 | CircleInterpolation_ReturnsToStart | Circular path | P2 |
| I007 | EmergencyStop_AllAxesStopSimultaneously | System-wide E-stop | P0 |

### Category 4: Fault Injection Tests

| Test ID | Test Name | Description | Priority |
|---------|-----------|-------------|----------|
| F001 | PortError_PropagatesToUseCase | Error propagation | P0 |
| F002 | HardwareDisconnect_ReportsError | Connection failure | P0 |
| F003 | EncoderSignalLoss_DetectsError | Sensor fault | P1 |
| F004 | CommunicationTimeout_RetriesConfigurable | Fault tolerance | P1 |
| F005 | PositionDeviation_CorrectsAutomatically | Error recovery | P2 |

---

## Test Templates

### Template 1: Unit Test with Mock

**Purpose:** Validate business logic with controlled mock behavior

```cpp
/**
 * @brief Template: Unit test with mock port
 *
 * Use this template when testing business logic that depends on port interfaces
 */
TEST_F(YourTestFixture, BusinessLogic_ValidInput_CallsPortCorrectly) {
    // Arrange
    SetupMockExpectations();

    InputParameters params = BuildValidParams();

    // Act
    auto result = system_under_test_->Execute(params);

    // Assert
    AssertResultSuccess(result);
    EXPECT_CALL(mock_port_, DesiredMethod(_, _))
        .Times(1);  // Verify port was called

    ValidateOutput(result.Value());
}
```

**Example - HomingController:**

```cpp
TEST_F(HomingControllerTest, StartHoming_ValidParameters_CallsPort) {
    // Arrange
    using testing::Return;

    auto params = HomingParametersBuilder()
        .Axis(1)
        .Mode(2)
        .Velocity(20.0f)
        .SearchVelocity(10.0f)
        .Build();

    EXPECT_CALL(*mock_port_, HomeAxis(1, testing::_))
        .WillOnce(Return(Result<void>::Success()));

    // Act
    auto result = controller_->StartHoming(1, params);

    // Assert
    AssertResultSuccess(result);
}
```

### Template 2: State Machine Test with Sim

**Purpose:** Verify state transitions with simulated time

```cpp
/**
 * @brief Template: State machine test with SimAxis
 *
 * Use this template when testing state transitions and timing
 */
TEST_F(SimAxisFixture, StateTransition_TriggeredByEvent_CompletesSuccessfully) {
    // Arrange
    SimulatedAxis axis(0, GetDefaultConfig());
    FakeClock clock;

    axis.Enable();

    // Act
    axis.Home();

    // Simulate time passing
    while (!axis.IsHomed()) {
        clock.tick(10ms);
        axis.Tick(0.01f);

        ASSERT_LT(clock.now() - start_time, timeout)
            << "State transition timeout";
    }

    // Assert
    AssertAxisState(axis, SimulatedAxis::State::Homed);
    EXPECT_FLOAT_EQ(axis.GetPosition(), 0.0f);
}
```

**Example - Successful Homing:**

```cpp
TEST_F(SimAxisTest, HomeSuccess_ShouldTransitionToHomedState) {
    // Arrange
    SimulatedAxis::Config config;
    config.max_velocity = 100.0f;
    config.max_acceleration = 500.0f;

    SimulatedAxis axis(0, config);
    FakeClock clock;

    axis.Enable();
    auto start_time = clock.now();
    auto timeout = std::chrono::milliseconds(5000);

    // Act
    axis.Home();

    // Simulate homing process
    while (!axis.IsHomed()) {
        clock.tick(std::chrono::milliseconds(10));
        axis.Tick(0.01f);

        ASSERT_LT(clock.now() - start_time, timeout)
            << "Homing did not complete within timeout";
    }

    // Assert
    AssertAxisState(axis, SimulatedAxis::State::Homed);
    EXPECT_FLOAT_EQ(axis.GetPosition(), 0.0f);
    EXPECT_FLOAT_EQ(axis.GetVelocity(), 0.0f);
}
```

### Template 3: Timeout Test

**Purpose:** Verify timeout handling

```cpp
/**
 * @brief Template: Timeout test
 *
 * Use this template when testing timeout scenarios
 */
TEST_F(TimeoutFixture, Operation_WhenTimeout_ShouldReturnError) {
    // Arrange
    SimulatedAxis axis(0, config_);
    FakeClock clock;
    int32_t timeout_ms = 1000;

    axis.Enable();

    // Configure mock to never complete
    MockNeverCompletingBehavior();

    // Act
    auto start = std::chrono::steady_clock::now();
    auto result = ExecuteWithTimeout(axis, timeout_ms, clock);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    );

    // Assert
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::OPERATION_TIMEOUT);

    // Should timeout close to specified time (±200ms tolerance)
    EXPECT_GE(duration.count(), timeout_ms);
    EXPECT_LT(duration.count(), timeout_ms + 200);
}
```

**Example - Homing Timeout:**

```cpp
TEST_F(HomingTestUseCaseTest, Timeout_SingleAxisTimeout) {
    // Arrange
    m_params.timeoutMs = 500;  // 500ms timeout
    m_mockPort->setHomingNeverCompletes(true);

    auto startTime = std::chrono::steady_clock::now();

    // Act
    auto result = m_useCase->executeHomingTest(m_params);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime
    );

    // Assert
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().code, ErrorCode::TEST_EXECUTION_FAILED);

    // Verify timeout duration is close to expected (±200ms)
    EXPECT_GE(duration.count(), 500);
    EXPECT_LT(duration.count(), 700);
}
```

### Template 4: Emergency Stop Test

**Purpose:** Verify emergency stop behavior

```cpp
/**
 * @brief Template: Emergency stop test
 *
 * Use this template when testing emergency stop behavior
 */
TEST_F(EmergencyStopFixture, EmergencyStop_DuringMotion_StopsImmediately) {
    // Arrange
    SimulatedAxis axis(0, config_);
    FakeClock clock;

    axis.Enable();
    axis.Home();
    WaitForHomed(axis, std::chrono::milliseconds(1000), clock);

    float32 start_position = axis.GetPosition();
    axis.MoveTo(100.0f, 50.0f);

    // Act - Emergency stop during motion
    clock.tick(std::chrono::milliseconds(100));
    axis.Tick(0.1f);
    ASSERT_EQ(axis.GetState(), SimulatedAxis::State::Moving);

    axis.EmergencyStop();

    // Assert
    AssertAxisState(axis, SimulatedAxis::State::EmergencyStopped);
    EXPECT_FLOAT_EQ(axis.GetVelocity(), 0.0f);

    // Position should not have reached target
    EXPECT_NE(axis.GetPosition(), 100.0f);
}
```

### Template 5: Parameterized Test

**Purpose:** Test multiple parameter combinations

```cpp
/**
 * @brief Template: Parameterized test
 *
 * Use this template when testing multiple parameter combinations
 */
class ParameterizedMotionTest : public MotionTestBase,
                                public ::testing::WithParamInterface<std::tuple<float32, float32, int32_t>> {
protected:
    float32 GetVelocity() const { return std::get<0>(GetParam()); }
    float32 GetAcceleration() const { return std::get<1>(GetParam()); }
    int32_t GetTimeout() const { return std::get<2>(GetParam()); }
};

TEST_P(ParameterizedMotionTest, MoveWithVariousParams_AllSucceed) {
    // Arrange
    auto velocity = GetVelocity();
    auto acceleration = GetAcceleration();
    auto timeout = GetTimeout();

    SimulatedAxis axis(0, config_);
    FakeClock clock;

    axis.Enable();
    axis.Home();
    WaitForHomed(axis, std::chrono::milliseconds(timeout), clock);

    // Act
    axis.MoveTo(100.0f, velocity);

    // Simulate until in position or timeout
    bool reached = WaitForInPosition(axis, std::chrono::milliseconds(timeout), clock);

    // Assert
    EXPECT_TRUE(reached) << "Failed with velocity=" << velocity
                        << ", accel=" << acceleration;
    AssertInPosition(axis, 100.0f, 0.1f);
}

// Instantiate test with parameter combinations
INSTANTIATE_TEST_SUITE_P(
    MotionVariations,
    ParameterizedMotionTest,
    ::testing::Values(
        std::make_tuple(50.0f, 250.0f, 3000),
        std::make_tuple(100.0f, 500.0f, 2000),
        std::make_tuple(200.0f, 1000.0f, 1500),
        std::make_tuple(400.0f, 2000.0f, 1000)
    )
);
```

### Template 6: Multi-Axis Coordination Test

**Purpose:** Verify coordinated motion

```cpp
/**
 * @brief Template: Multi-axis coordination test
 *
 * Use this template when testing multi-axis coordinated motion
 */
TEST_F(CoordinationTest, MultiAxisSync_StartAndStopTogether) {
    // Arrange
    std::vector<SimulatedAxis> axes;
    for (int i = 0; i < 4; ++i) {
        axes.emplace_back(i, config_);
        axes[i].Enable();
        axes[i].Home();
    }

    FakeClock clock;

    // Wait for all axes to home
    for (auto& axis : axes) {
        WaitForHomed(axis, std::chrono::milliseconds(1000), clock);
    }

    // Act - Start all axes simultaneously
    auto start_positions = std::vector<float32>{0.0f, 0.0f, 0.0f, 0.0f};
    auto target_positions = std::vector<float32>{100.0f, 50.0f, 75.0f, 25.0f};

    for (size_t i = 0; i < axes.size(); ++i) {
        axes[i].MoveTo(target_positions[i], 100.0f);
    }

    // Simulate coordinated motion
    bool all_in_position = false;
    auto timeout = std::chrono::milliseconds(5000);
    auto start_time = clock.now();

    while (!all_in_position && (clock.now() - start_time) < timeout) {
        clock.tick(std::chrono::milliseconds(10));
        for (auto& axis : axes) {
            axis.Tick(0.01f);
        }

        all_in_position = std::all_of(axes.begin(), axes.end(),
            [](const SimulatedAxis& a) { return a.IsInPosition(); });
    }

    // Assert
    EXPECT_TRUE(all_in_position);

    for (size_t i = 0; i < axes.size(); ++i) {
        AssertInPosition(axes[i], target_positions[i], 0.1f);
    }
}
```

### Template 7: CMP Trigger Test

**Purpose:** Verify CMP (Compare) trigger functionality

```cpp
/**
 * @brief Template: CMP trigger test
 *
 * Use this template when testing CMP trigger functionality
 */
TEST_F(CMPTriggerTest, TriggerAtPosition_FiresAtCorrectLocation) {
    // Arrange
    SimulatedAxis axis(0, config_);
    FakeClock clock;

    axis.Enable();
    axis.Home();
    WaitForHomed(axis, std::chrono::milliseconds(1000), clock);

    // Configure trigger at 50mm
    float32 trigger_position = 50.0f;
    int trigger_id = 0;
    bool trigger_fired = false;

    axis.ConfigureTrigger(trigger_position, [&trigger_fired]() {
        trigger_fired = true;
    });

    // Act
    axis.MoveTo(100.0f, 100.0f);

    // Simulate motion
    while (!axis.IsInPosition()) {
        clock.tick(std::chrono::milliseconds(10));
        float32 old_pos = axis.GetPosition();
        axis.Tick(0.01f);
        float32 new_pos = axis.GetPosition();

        // Check if trigger fired near expected position
        if (trigger_fired) {
            EXPECT_NEAR(new_pos, trigger_position, 1.0f);
            break;
        }
    }

    // Assert
    EXPECT_TRUE(trigger_fired) << "CMP trigger did not fire";
}
```

---

## Fault Injection Templates

### Template 1: Port Error Propagation

```cpp
/**
 * @brief Template: Port error propagation test
 *
 * Verifies that errors from port are correctly propagated
 */
TEST_F(FaultInjectionTest, PortError_PropagatesToUseCase) {
    // Arrange
    auto expected_error = Error(
        ErrorCode::HARDWARE_NOT_CONNECTED,
        "Simulated hardware failure"
    );

    EXPECT_CALL(*mock_port_, HomeAxis(_, _))
        .WillOnce(Return(Result<void>::Failure(expected_error)));

    HomingParameters params = BuildValidParams();

    // Act
    auto result = controller_->StartHoming(1, params);

    // Assert
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::HARDWARE_NOT_CONNECTED);
    EXPECT_STREQ(result.GetError().GetMessage().c_str(),
                 "Simulated hardware failure");
}
```

### Template 2: Intermittent Fault

```cpp
/**
 * @brief Template: Intermittent fault test
 *
 * Verifies system behavior with intermittent failures
 */
TEST_F(FaultInjectionTest, IntermittentFault_RetriesSucceed) {
    // Arrange
    int attempt_count = 0;
    auto retry_behavior = [&attempt_count]() -> Result<void> {
        if (++attempt_count < 3) {
            return Result<void>::Failure(
                Error(ErrorCode::COMMUNICATION_TIMEOUT, "Timeout")
            );
        }
        return Result<void>::Success();
    };

    EXPECT_CALL(*mock_port_, HomeAxis(_, _))
        .WillRepeatedly(testing::Invoke(retry_behavior));

    HomingParameters params = BuildValidParams();

    // Act
    auto result = controller_->StartHoming(1, params);

    // Assert
    AssertResultSuccess(result);
    EXPECT_EQ(attempt_count, 3);  // Failed twice, succeeded on third try
}
```

### Template 3: Hardware Disconnect During Operation

```cpp
/**
 * @brief Template: Hardware disconnect during operation
 *
 * Verifies graceful handling of hardware disconnect
 */
TEST_F(FaultInjectionTest, DisconnectDuringOperation_DetectsAndReports) {
    // Arrange
    SimulatedAxis axis(0, config_);
    FakeClock clock;

    axis.Enable();
    axis.Home();
    WaitForHomed(axis, std::chrono::milliseconds(1000), clock));

    // Act - Start motion
    axis.MoveTo(100.0f, 100.0f);

    // Simulate disconnect during motion
    clock.tick(std::chrono::milliseconds(50));
    axis.Tick(0.05f);

    axis.SimulateHardwareDisconnect();

    // Continue simulation
    clock.tick(std::chrono::milliseconds(100));
    axis.Tick(0.1f);

    // Assert
    EXPECT_TRUE(axis.IsError());
    EXPECT_EQ(axis.GetState(), SimulatedAxis::State::Error);
}
```

---

## Implementation Guidelines

### 1. Test Naming Convention

Follow the pattern: `Scenario_ExpectedBehavior_StateUnderTest`

**Examples:**
- ✅ `HomeSuccess_ShouldTransitionToHomedState`
- ✅ `MoveExceedsSoftLimit_ShouldTriggerAlarm`
- ✅ `EmergencyStopDuringMove_ShouldStopImmediately`
- ❌ `Test1` - Too vague
- ❌ `HomeWorks` - Not specific enough

### 2. Test Structure (AAA Pattern)

All tests should follow Arrange-Act-Assert structure:

```cpp
TEST_F(YourFixture, TestName) {
    // ============ Arrange ============
    // Setup test data, configure mocks, initialize system

    // ============ Act ============
    // Execute the operation under test

    // ============ Assert ============
    // Verify expected outcomes
}
```

### 3. Assertion Best Practices

- **Prefer EXPECT over ASSERT** for non-critical checks to allow multiple failures per test
- **Use ASSERT for setup validation** to stop if prerequisites aren't met
- **Provide meaningful messages** with assertions
- **Use type-safe assertions** (EXPECT_EQ vs EXPECT_TRUE)

```cpp
// Good
EXPECT_EQ(axis.GetPosition(), 100.0f) << "Axis should reach target position";

// Avoid
EXPECT_TRUE(axis.GetPosition() == 100.0f);  // No error message
```

### 4. Test Independence

- Each test must be independent
- Use `SetUp()` and `TearDown()` for isolation
- Don't rely on test execution order
- Clean up resources in `TearDown()`

### 5. Mock Usage Guidelines

- **Mock only external dependencies** (ports, adapters)
- **Don't mock the system under test**
- **Set clear expectations** before calling SUT
- **Verify expectations** automatically via gmock

```cpp
// Good
EXPECT_CALL(*mock_port_, HomeAxis(1, _))
    .Times(1)
    .WillOnce(Return(Result<void>::Success()));
controller_->StartHoming(1, params);
// Verification is automatic when mock_port_ is destroyed

// Avoid - too brittle
EXPECT_CALL(*mock_port_, HomeAxis(1, _))
    .Times(1)
    .WillOnce(Return(Result<void>::Success()));
EXPECT_CALL(*mock_port_, GetHomingStatus(1))
    .Times(Exactly(5));  // Too specific, will break with timing changes
```

### 6. Time-Sensitive Tests

For tests involving timing:

- **Use FakeClock** for deterministic tests
- **Never use sleep/wait** in unit tests
- **Set reasonable timeouts** for integration tests
- **Document timeout assumptions**

```cpp
// Good - uses FakeClock
FakeClock clock;
axis.Home();
while (!axis.IsHomed()) {
    clock.tick(10ms);
    axis.Tick(0.01f);
    ASSERT_LT(clock.now() - start, timeout);
}

// Avoid - non-deterministic timing
axis.Home();
std::this_thread::sleep_for(std::chrono::milliseconds(100));
EXPECT_TRUE(axis.IsHomed());
```

### 7. Test Coverage Goals

- **Unit Tests**: > 85% line coverage
- **Branch Coverage**: > 75% for critical paths
- **State Transitions**: 100% coverage
- **Error Paths**: All error codes tested

### 8. Performance Testing

For performance-critical code:

```cpp
TEST_F(PerformanceTest, Interpolation_1000Points_CompletesInTime) {
    // Arrange
    std::vector<Point2D> points = GenerateTrajectory(1000);

    auto start = std::chrono::high_resolution_clock::now();

    // Act
    auto result = interpolator_->CalculatePath(points);

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start
    );

    // Assert
    AssertResultSuccess(result);
    EXPECT_LT(duration.count(), 10000)  // < 10ms
        << "Interpolation took " << duration.count() << "μs";
}
```

---

## Appendix: Test Implementation Checklist

When implementing new tests, verify:

- [ ] Test follows AAA pattern
- [ ] Test name is descriptive
- [ ] Mock expectations are clear
- [ ] Assertions have meaningful messages
- [ ] Test is independent (no shared state)
- [ ] Test cleans up resources
- [ ] Test is deterministic (no random behavior)
- [ ] Test is fast (< 100ms for unit tests)
- [ ] Test documents timeout assumptions (if applicable)
- [ ] Test covers both success and failure paths

---

## Summary

This test suite design provides:

1. **Comprehensive Coverage**: Unit, state machine, integration, and fault injection tests
2. **Reusable Components**: Base fixtures, helpers, and builders
3. **Clear Templates**: Proven patterns for common test scenarios
4. **Best Practices**: Guidelines for writing maintainable tests
5. **Deterministic Testing**: FakeClock and SimAxis for reliable timing tests

**Next Steps:**
1. Review and approve test architecture
2. Implement base fixtures (MotionTestBase, SimulatedAxis, FakeClock)
3. Implement helper utilities (TestAssertions, TestBuilders)
4. Implement priority P0 tests from catalog
5. Expand coverage to P1 and P2 tests
6. Integrate with CI/CD pipeline
7. Generate coverage reports

---

**Document Version History:**
- v1.0.0 (2026-01-12): Initial design draft

