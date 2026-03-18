#include "siligen/gateway/tcp/tcp_server_host.h"

#include "tcp/TcpCommandDispatcher.h"
#include "tcp/TcpServer.h"

namespace Siligen::Gateway::Tcp {

TcpServerHost::TcpServerHost(
    boost::asio::io_context& io_context,
    const TcpFacadeBundle& facades,
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
    TcpServerHostOptions options)
    : dispatcher_(std::make_unique<Adapters::Tcp::TcpCommandDispatcher>(
          facades.system,
          facades.motion,
          facades.dispensing,
          facades.recipe,
          std::move(config_port),
          std::move(options.config_path))),
      server_(std::make_shared<Adapters::Tcp::TcpServer>(io_context, options.port, *dispatcher_)) {}

TcpServerHost::~TcpServerHost() = default;

void TcpServerHost::Start() {
    if (server_) {
        server_->Start();
    }
}

void TcpServerHost::Stop() {
    if (server_) {
        server_->Stop();
    }
}

}  // namespace Siligen::Gateway::Tcp
