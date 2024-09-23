#ifndef DIPLO_RPC_SERVER_HPP
#define DIPLO_RPC_SERVER_HPP

#include <memory>
#include "net/rpc_router.hpp"

#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/signal_set.hpp>
#include <asio/write.hpp>

#include <nlohmann/json.hpp>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using asio::ip::tcp;
namespace this_coro = asio::this_coro;

using json = nlohmann::json;

class RPCServer
{
public:
    RPCServer() = default;
    RPCServer(RPCRouter &router, asio::io_context &ctx, json &config);

    void setup();

    awaitable<void> listener();

    awaitable<void> handle_connection(tcp::socket socket);

    std::string listening_address;
    int listening_port;

private:
    RPCRouter &router_;

    asio::io_context &io_context;

    json config_;
};

#endif