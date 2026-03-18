#include "sim/scheme_c/virtual_clock.h"

#include <sstream>
#include <utility>

namespace sim::scheme_c {

namespace {

std::string buildVirtualTimeErrorMessage(VirtualTimeErrorCode code,
                                         const std::string& operation,
                                         const std::string& detail,
                                         const TickInfo& tick,
                                         const std::optional<Duration>& timeout) {
    std::ostringstream stream;
    stream << "VirtualTimeError{code=";

    switch (code) {
    case VirtualTimeErrorCode::InvalidStep:
        stream << "InvalidStep";
        break;
    case VirtualTimeErrorCode::InvalidTickCount:
        stream << "InvalidTickCount";
        break;
    case VirtualTimeErrorCode::InvalidTimeout:
        stream << "InvalidTimeout";
        break;
    case VirtualTimeErrorCode::AlreadyStarted:
        stream << "AlreadyStarted";
        break;
    case VirtualTimeErrorCode::NotRunning:
        stream << "NotRunning";
        break;
    case VirtualTimeErrorCode::NotPaused:
        stream << "NotPaused";
        break;
    case VirtualTimeErrorCode::AlreadyStopped:
        stream << "AlreadyStopped";
        break;
    case VirtualTimeErrorCode::TimeoutReached:
        stream << "TimeoutReached";
        break;
    }

    stream << ", operation=" << operation
           << ", detail=" << detail
           << ", tick_index=" << tick.tick_index
           << ", now_ns=" << tick.now.count()
           << ", step_ns=" << tick.step.count();
    if (timeout.has_value()) {
        stream << ", timeout_ns=" << timeout->count();
    }
    stream << "}";
    return stream.str();
}

}  // namespace

VirtualTimeError::VirtualTimeError(VirtualTimeErrorCode code,
                                   std::string operation,
                                   std::string detail,
                                   TickInfo tick,
                                   std::optional<Duration> timeout)
    : std::runtime_error(buildVirtualTimeErrorMessage(code, operation, detail, tick, timeout)),
      code_(code),
      operation_(std::move(operation)),
      detail_(std::move(detail)),
      tick_(tick),
      timeout_(timeout) {}

VirtualTimeErrorCode VirtualTimeError::code() const noexcept {
    return code_;
}

const std::string& VirtualTimeError::operation() const noexcept {
    return operation_;
}

const std::string& VirtualTimeError::detail() const noexcept {
    return detail_;
}

const TickInfo& VirtualTimeError::tick() const noexcept {
    return tick_;
}

const std::optional<Duration>& VirtualTimeError::timeout() const noexcept {
    return timeout_;
}

VirtualClock::VirtualClock(Duration step) {
    if (step <= Duration::zero()) {
        throw VirtualTimeError(
            VirtualTimeErrorCode::InvalidStep,
            "VirtualClock::VirtualClock",
            "VirtualClock step must be positive.",
            TickInfo{0, Duration::zero(), step});
    }

    current_.step = step;
}

void VirtualClock::reset() {
    current_.tick_index = 0;
    current_.now = Duration::zero();
    timeout_.reset();
    state_ = VirtualClockState::Idle;
}

void VirtualClock::start(std::optional<Duration> timeout) {
    if (state_ != VirtualClockState::Idle) {
        throwError(
            VirtualTimeErrorCode::AlreadyStarted,
            "VirtualClock::start",
            "Clock must be reset before it can be started again.");
    }

    if (timeout.has_value() && *timeout <= Duration::zero()) {
        throw VirtualTimeError(
            VirtualTimeErrorCode::InvalidTimeout,
            "VirtualClock::start",
            "Timeout must be positive when provided.",
            current_,
            timeout);
    }

    timeout_ = timeout;
    state_ = VirtualClockState::Running;
}

void VirtualClock::pause() {
    if (state_ != VirtualClockState::Running) {
        throwError(
            VirtualTimeErrorCode::NotRunning,
            "VirtualClock::pause",
            "Clock can only be paused while running.");
    }

    state_ = VirtualClockState::Paused;
}

void VirtualClock::resume() {
    if (state_ != VirtualClockState::Paused) {
        throwError(
            VirtualTimeErrorCode::NotPaused,
            "VirtualClock::resume",
            "Clock can only be resumed from the paused state.");
    }

    state_ = VirtualClockState::Running;
}

void VirtualClock::stop() {
    if (state_ == VirtualClockState::Stopped) {
        throwError(
            VirtualTimeErrorCode::AlreadyStopped,
            "VirtualClock::stop",
            "Clock has already been stopped.");
    }

    if (state_ == VirtualClockState::Idle) {
        throwError(
            VirtualTimeErrorCode::NotRunning,
            "VirtualClock::stop",
            "Clock has not been started.");
    }

    state_ = VirtualClockState::Stopped;
}

const TickInfo& VirtualClock::current() const {
    return current_;
}

VirtualClockState VirtualClock::state() const noexcept {
    return state_;
}

std::optional<Duration> VirtualClock::timeout() const noexcept {
    return timeout_;
}

bool VirtualClock::isRunning() const noexcept {
    return state_ == VirtualClockState::Running;
}

bool VirtualClock::isPaused() const noexcept {
    return state_ == VirtualClockState::Paused;
}

bool VirtualClock::isStopped() const noexcept {
    return state_ == VirtualClockState::Stopped;
}

bool VirtualClock::hasTimedOut() const noexcept {
    return state_ == VirtualClockState::TimedOut;
}

void VirtualClock::advance(TickIndex ticks) {
    if (ticks == 0) {
        throwError(
            VirtualTimeErrorCode::InvalidTickCount,
            "VirtualClock::advance",
            "Advance requires at least one logical tick.");
    }

    if (state_ != VirtualClockState::Running) {
        throwError(
            VirtualTimeErrorCode::NotRunning,
            "VirtualClock::advance",
            "Clock can only advance while running.");
    }

    for (TickIndex index = 0; index < ticks; ++index) {
        current_.tick_index += 1;
        current_.now += current_.step;

        if (timeout_.has_value() && current_.now >= *timeout_) {
            state_ = VirtualClockState::TimedOut;
            throw VirtualTimeError(
                VirtualTimeErrorCode::TimeoutReached,
                "VirtualClock::advance",
                "Logical timeout reached during tick advancement.",
                current_,
                timeout_);
        }
    }
}

bool VirtualClock::hasReached(Duration timeout) const {
    return current_.now >= timeout;
}

[[noreturn]] void VirtualClock::throwError(VirtualTimeErrorCode code,
                                           const std::string& operation,
                                           const std::string& detail) const {
    throw VirtualTimeError(code, operation, detail, current_, timeout_);
}

}  // namespace sim::scheme_c
