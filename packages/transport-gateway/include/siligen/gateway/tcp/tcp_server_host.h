#pragma once

#include "siligen/gateway/tcp/tcp_facade_bundle.h"

#include <boost/asio.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace Siligen::Adapters::Tcp {
class TcpCommandDispatcher;
class TcpServer;
}

namespace Siligen::Domain::Configuration::Ports {
class IConfigurationPort;
}

namespace Siligen::Gateway::Tcp {

struct TcpServerHostOptions {
    uint16_t port = 9527;
    std::string config_path = "config/machine/machine_config.ini";
};

class TcpServerHost {
   public:
    TcpServerHost(
        boost::asio::io_context& io_context,
        const TcpFacadeBundle& facades,
        std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
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
