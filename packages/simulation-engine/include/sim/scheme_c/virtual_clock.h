#pragma once

#include <optional>
#include <stdexcept>
#include <string>

#include "sim/scheme_c/types.h"

namespace sim::scheme_c {

enum class VirtualClockState {
    Idle,
    Running,
    Paused,
    Stopped,
    TimedOut
};

enum class VirtualTimeErrorCode {
    InvalidStep,
    InvalidTickCount,
    InvalidTimeout,
    AlreadyStarted,
    NotRunning,
    NotPaused,
    AlreadyStopped,
    TimeoutReached
};

class VirtualTimeError : public std::runtime_error {
public:
    VirtualTimeError(VirtualTimeErrorCode code,
                     std::string operation,
                     std::string detail,
                     TickInfo tick,
                     std::optional<Duration> timeout = std::nullopt);

    VirtualTimeErrorCode code() const noexcept;
    const std::string& operation() const noexcept;
    const std::string& detail() const noexcept;
    const TickInfo& tick() const noexcept;
    const std::optional<Duration>& timeout() const noexcept;

private:
    VirtualTimeErrorCode code_;
    std::string operation_;
    std::string detail_;
    TickInfo tick_{};
    std::optional<Duration> timeout_{};
};

class VirtualClock {
public:
    explicit VirtualClock(Duration step = std::chrono::milliseconds(1));

    void reset();
    void start(std::optional<Duration> timeout = std::nullopt);
    void pause();
    void resume();
    void stop();
    const TickInfo& current() const;
    VirtualClockState state() const noexcept;
    std::optional<Duration> timeout() const noexcept;
    bool isRunning() const noexcept;
    bool isPaused() const noexcept;
    bool isStopped() const noexcept;
    bool hasTimedOut() const noexcept;
    void advance(TickIndex ticks = 1);
    bool hasReached(Duration timeout) const;

private:
    [[noreturn]] void throwError(VirtualTimeErrorCode code,
                                 const std::string& operation,
                                 const std::string& detail) const;

    TickInfo current_{};
    VirtualClockState state_{VirtualClockState::Idle};
    std::optional<Duration> timeout_{};
};

}  // namespace sim::scheme_c
