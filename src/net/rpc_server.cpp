#include "net/rpc_server.hpp"

RPCServer::RPCServer(RPCRouter &router, asio::io_context &ctx, json &config) : router_(router), io_context(ctx), config_(config)
{
    listening_address = config_["net"]["rpc_address"];
    listening_port = config_["net"]["rpc_port"];
}

void RPCServer::setup()
{
    co_spawn(io_context, listener(), detached);
}

awaitable<void> RPCServer::listener()
{

    std::cout << "started RPC server, listening on: " << listening_address << ":" << listening_port << std::endl;
    auto executor = co_await this_coro::executor;
    tcp::acceptor acceptor(executor, {asio::ip::address::from_string(listening_address), static_cast<short unsigned int>(listening_port)});

    for (;;)
    {
        tcp::socket socket = co_await acceptor.async_accept(use_awaitable);

        co_spawn(executor, handle_connection(std::move(socket)), [this](std::exception_ptr e)
                 {
            if (e)
            {

            try
            {
                std::rethrow_exception(e);
            } catch (std::exception &exc)
            {
                // remove peer from inbound connections when the listen fails
            }} });
    }
}

awaitable<void> RPCServer::handle_connection(tcp::socket socket)
{
    // For JSON RPC, only header will be the total length: 4 bytes
    unsigned char header[4];

    std::vector<unsigned char> v_data;

    // first read header
    std::size_t n = 0;
    while (n != 4)
    {
        n += co_await socket.async_read_some(asio::buffer(header + n, 4 - n), use_awaitable);
    }

    int msg_len = util::chars_to_int32_big_endian(header);

    // allocate bytes to save incoming data
    v_data.resize(msg_len);

    n = 0;
    while (n != msg_len)
    {
        n += co_await socket.async_read_some(asio::buffer(v_data.data() + n, msg_len - n), use_awaitable);
    }

    // convert bytes to string
    std::string json_str(v_data.begin(), v_data.end());

    // parse as json
    json msg_json = json::parse(json_str);

    auto resp_json = router_.route(msg_json);
    if (!resp_json.empty())
    {
        auto resp_str = resp_json.dump();
        n = 0;
        while (n != resp_str.size())
        {
            n += co_await async_write(socket, asio::buffer(resp_str.c_str() + n, resp_str.size() - n), use_awaitable);
        }
    }

    // RPCs are supposed to be one-off requests, so here let the socket go out of scope and close after writing response
}