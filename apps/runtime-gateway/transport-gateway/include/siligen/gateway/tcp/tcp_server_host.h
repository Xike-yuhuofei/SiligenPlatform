#pragma once

#include "siligen/gateway/tcp/tcp_facade_bundle.h"
#include <boost/asio.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace Siligen::Adapters::Tcp {
class TcpCommandDispatcher;
class MockIoControlService;
class TcpServer;
}

namespace Siligen::Domain::Configuration::Ports {
class IConfigurationPort;
}

namespace Siligen::RuntimeExecution::Contracts::System {
class IRuntimeStatusPort;
}

namespace Siligen::Gateway::Tcp {

struct TcpServerHostOptions {
    uint16_t port = 9527;
};

class TcpServerHost {
   public:
    TcpServerHost(
        boost::asio::io_context& io_context,
        const TcpFacadeBundle& facades,
        std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
        std::shared_ptr<RuntimeExecution::Contracts::System::IRuntimeStatusPort> runtime_status_port,
        std::shared_ptr<Adapters::Tcp::MockIoControlService> mock_io_control,
        TcpServerHostOptions options);

    ~TcpServerHost();

    TcpServerHost(const TcpServerHost&) = delete;
    TcpServerHost& operator=(const TcpServerHost&) = delete;

    void Start();
    void Stop();

   private:
    std::unique_ptr<Adapters::Tcp::TcpCommandDispatcher> dispatcher_;
    std::shared_ptr<Adapters::Tcp::TcpServer> server_;
};

}  // namespace Siligen::Gateway::Tcp
