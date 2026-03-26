#pragma once

#include "siligen/gateway/protocol/request_envelope.h"
#include "siligen/gateway/protocol/response_envelope.h"

namespace Siligen::Gateway::Dispatch {

class CommandDispatchService {
   public:
    virtual ~CommandDispatchService() = default;

    virtual Siligen::Gateway::Protocol::ResponseEnvelope Dispatch(
        const Siligen::Gateway::Protocol::RequestEnvelope& request) = 0;
};

}  // namespace Siligen::Gateway::Dispatch
