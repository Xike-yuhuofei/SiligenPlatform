#pragma once

#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <memory>

namespace Siligen::Shared::Util {

template <typename TPort>
inline Types::Result<void> EnsurePort(const std::shared_ptr<TPort>& port,
                                      const char* message,
                                      const char* source,
                                      Types::ErrorCode code = Types::ErrorCode::PORT_NOT_INITIALIZED) {
    if (port) {
        return Types::Result<void>::Success();
    }
    return Types::Result<void>::Failure(Types::Error(code, message, source));
}

}  // namespace Siligen::Shared::Util
