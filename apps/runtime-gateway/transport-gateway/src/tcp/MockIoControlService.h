#pragma once

#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <optional>
#include <string>

namespace Siligen::Infrastructure::Hardware {
class MockMultiCardWrapper;
}

namespace Siligen::Adapters::Tcp {

struct MockIoState {
    bool estop = false;
    bool door = false;
    bool limit_x_neg = false;
    bool limit_y_neg = false;
};

struct MockIoUpdateRequest {
    std::optional<bool> estop;
    std::optional<bool> door;
    std::optional<bool> limit_x_neg;
    std::optional<bool> limit_y_neg;
};

class MockIoControlService {
   public:
    MockIoControlService(
        std::string config_path,
        Siligen::Infrastructure::Hardware::MockMultiCardWrapper* mock_multicard,
        bool mock_mode_enabled);

    Shared::Types::Result<MockIoState> Apply(const MockIoUpdateRequest& request) const;

   private:
    Shared::Types::Result<MockIoState> ReadState() const;
    Shared::Types::Result<long> ReadInputRaw(short group) const;
    Shared::Types::Result<void> WriteInputRaw(short group, long value) const;

    std::string config_path_;
    Siligen::Infrastructure::Hardware::MockMultiCardWrapper* mock_multicard_ = nullptr;
    bool mock_mode_enabled_ = false;
    short estop_input_ = 0;
    bool estop_active_low_ = true;
    short door_input_ = -1;
};

}  // namespace Siligen::Adapters::Tcp
