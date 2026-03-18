#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <string>

namespace Siligen::Adapters::Tcp {

class TcpCommandDispatcher;
class TcpServer;

/**
 * @brief TCP会话 - 处理单个客户端连接
 *
 * 负责：
 * - 异步读取客户端消息（以\n分隔）
 * - 将消息转发给CommandDispatcher处理
 * - 异步发送响应
 */
class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    TcpSession(boost::asio::ip::tcp::socket socket,
               TcpCommandDispatcher& dispatcher,
               TcpServer& server);

    ~TcpSession() = default;

    /// 启动会话（开始读取）
    void Start();

    /// 关闭会话
    void Close();

    /// 发送消息到客户端
    void Send(const std::string& message);

    /// 获取远程端点信息
    std::string GetRemoteEndpoint() const;

private:
    /// 异步读取一行
    void DoRead();

    /// 处理接收到的消息
    void ProcessMessage(const std::string& message);

    boost::asio::ip::tcp::socket socket_;
    TcpCommandDispatcher& dispatcher_;
    TcpServer& server_;
    boost::asio::streambuf read_buffer_;
    bool closed_ = false;
};

} // namespace Siligen::Adapters::Tcp
