#include "TcpServer.h"
#include "TcpSession.h"
#include <iostream>

namespace Siligen::Adapters::Tcp {

TcpServer::TcpServer(boost::asio::io_context& io_context,
                     uint16_t port,
                     TcpCommandDispatcher& dispatcher)
    : acceptor_(io_context, boost::asio::ip::tcp::endpoint(
          boost::asio::ip::tcp::v4(), port))
    , dispatcher_(dispatcher) {
}

void TcpServer::Start() {
    running_ = true;
    DoAccept();
    std::cout << "[TCP Server] Listening on port "
              << acceptor_.local_endpoint().port() << std::endl;
}

void TcpServer::Stop() {
    running_ = false;
    acceptor_.close();
    for (auto& session : sessions_) {
        session->Close();
    }
    sessions_.clear();
}

void TcpServer::RemoveSession(std::shared_ptr<TcpSession> session) {
    sessions_.erase(session);
}

void TcpServer::DoAccept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec,
               boost::asio::ip::tcp::socket socket) {
            if (!ec && running_) {
                auto session = std::make_shared<TcpSession>(
                    std::move(socket), dispatcher_, *this);
                sessions_.insert(session);
                session->Start();
                std::cout << "[TCP Server] Client connected: "
                          << session->GetRemoteEndpoint() << std::endl;
            }
            if (running_) {
                DoAccept();
            }
        });
}

} // namespace Siligen::Adapters::Tcp
