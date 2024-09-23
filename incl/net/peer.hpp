#ifndef DIPLO_PEER_HPP
#define DIPLO_PEER_HPP

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/signal_set.hpp>
#include <asio/write.hpp>

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

#include "net/router.hpp"

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using asio::ip::tcp;

class Peer
{
public:
    Peer() = default;
    Peer(Router &router, tcp::socket socket, asio::io_context &ctx);

    awaitable<void> listen();
    std::string ID();

    std::vector<unsigned char> send(std::vector<unsigned char> &data, bool expecting_resp = false);
    awaitable<void> async_send(std::shared_ptr<std::vector<unsigned char>> data_ptr);
    std::pair<std::string, int> get_real_addr();
    std::pair<std::string, int> get_listening_addr();

    void update_addr_info(std::string &new_addr, int new_port);

private:
    asio::io_context &io_context;

    tcp::socket _socket;
    std::mutex _socket_mu;

    std::string l_addr;
    int l_port;

    std::mutex _state_mu;

    std::string _id;
    Router &router_;

    std::mutex _async_cv_mu;
    std::condition_variable _async_cv;
    bool _sync_is_running;

    void pause_async_read();
    void resume_async_read();
};

#endif