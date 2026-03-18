#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <set>
#include <cstdint>

namespace Siligen::Adapters::Tcp {

class TcpSession;
class TcpCommandDispatcher;

/**
 * @brief TCP服务器 - 接受HMI客户端连接
 *
 * 使用Boost.Asio实现异步TCP服务器，支持多客户端连接。
 * 作为Driving Adapter，将TCP请求转发给UseCase处理。
 */
class TcpServer : public std::enable_shared_from_this<TcpServer> {
public:
    /**
     * @brief 构造函数
     * @param io_context Boost.Asio IO上下文
     * @param port 监听端口
     * @param dispatcher 命令分发器引用
     */
    TcpServer(boost::asio::io_context& io_context,
              uint16_t port,
              TcpCommandDispatcher& dispatcher);

    ~TcpServer() = default;

    /// 启动服务器
    void Start();

    /// 停止服务器
    void Stop();

    /// 移除已断开的会话
    void RemoveSession(std::shared_ptr<TcpSession> session);

private:
    /// 开始接受连接
    void DoAccept();

    boost::asio::ip::tcp::acceptor acceptor_;
    TcpCommandDispatcher& dispatcher_;
    std::set<std::shared_ptr<TcpSession>> sessions_;
    bool running_ = false;
};

} // namespace Siligen::Adapters::Tcp
