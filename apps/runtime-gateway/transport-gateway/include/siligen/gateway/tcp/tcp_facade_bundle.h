#pragma once

#include <memory>

namespace Siligen::Application::Facades::Tcp {
class TcpDispensingFacade;
class TcpMotionFacade;
class TcpSystemFacade;
}

namespace Siligen::Gateway::Tcp {

struct TcpFacadeBundle {
    std::shared_ptr<Application::Facades::Tcp::TcpSystemFacade> system;
    std::shared_ptr<Application::Facades::Tcp::TcpMotionFacade> motion;
    std::shared_ptr<Application::Facades::Tcp::TcpDispensingFacade> dispensing;
};

}  // namespace Siligen::Gateway::Tcp
