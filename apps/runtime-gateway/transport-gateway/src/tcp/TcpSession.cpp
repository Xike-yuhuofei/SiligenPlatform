#include "TcpSession.h"
#include "TcpServer.h"
#include "TcpCommandDispatcher.h"
#include "shared/types/TraceContext.h"
#include <iostream>
#include <sstream>

namespace Siligen::Adapters::Tcp {

TcpSession::TcpSession(boost::asio::ip::tcp::socket socket,
                       TcpCommandDispatcher& dispatcher,
                       TcpServer& server)
    : socket_(std::move(socket))
    , dispatcher_(dispatcher)
    , server_(server) {
}

void TcpSession::Start() {
    DoRead();
}

void TcpSession::Close() {
    if (!closed_) {
        closed_ = true;
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }
}

void TcpSession::Send(const std::string& message) {
    auto self = shared_from_this();
    auto msg = std::make_shared<std::string>(message + "\n");

    boost::asio::async_write(socket_, boost::asio::buffer(*msg),
        [self, msg](boost::system::error_code ec, std::size_t /*length*/) {
            if (ec) {
                std::cerr << "[TCP Session] Write error: " << ec.message() << std::endl;
            }
        });
}

std::string TcpSession::GetRemoteEndpoint() const {
    try {
        auto endpoint = socket_.remote_endpoint();
        std::ostringstream oss;
        oss << endpoint.address().to_string() << ":" << endpoint.port();
        return oss.str();
    } catch (...) {
        return "unknown";
    }
}

void TcpSession::DoRead() {
    auto self = shared_from_this();

    boost::asio::async_read_until(socket_, read_buffer_, '\n',
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                std::istream is(&read_buffer_);
                std::string line;
                std::getline(is, line);

                // 移除可能的\r
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (!line.empty()) {
                    ProcessMessage(line);
                }

                DoRead();
            } else {
                if (ec != boost::asio::error::eof &&
                    ec != boost::asio::error::operation_aborted) {
                    std::cerr << "[TCP Session] Read error: " << ec.message() << std::endl;
                }
                std::cout << "[TCP Server] Client disconnected: "
                          << GetRemoteEndpoint() << std::endl;
                server_.RemoveSession(self);
            }
        });
}

void TcpSession::ProcessMessage(const std::string& message) {
    Siligen::Shared::Types::TraceContext::Clear();
    Siligen::Shared::Types::TraceContext::SetAttribute("session_id", GetRemoteEndpoint());
    auto response = dispatcher_.Dispatch(message);
    Siligen::Shared::Types::TraceContext::Clear();
    Send(response);
}

} // namespace Siligen::Adapters::Tcp
