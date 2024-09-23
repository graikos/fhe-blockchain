#include "net/peer.hpp"
#include "net/router.hpp"
#include "util/util.hpp"

#include <iostream>
#include <sstream>
#include <thread>
#include <condition_variable>
#include <mutex>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::use_awaitable;
using asio::ip::tcp;

namespace this_coro = asio::this_coro;

Peer::Peer(Router &router, tcp::socket socket, asio::io_context &ctx) : router_(router), _socket(std::move(socket)), io_context(ctx), _sync_is_running(false)
{
    std::ostringstream addr;
    addr << _socket.remote_endpoint().address().to_string() << ":" << _socket.remote_endpoint().port();
    _id = addr.str();
}

awaitable<void> Peer::listen()
{
    // TODO: manager needs to know when when the stream closes and remove the peer from the maps
    // maybe it needs to pass the maps
    // message size: 4 bytes
    // message type: 4 bytes

    unsigned char header_data[8];
    std::vector<unsigned char> v_data;

    for (;;)
    {

        // in case of sync read taking place, wait here
        {
            std::unique_lock<std::mutex> lock(_async_cv_mu);
            _async_cv.wait(lock, [this]
                           { return !_sync_is_running; });
        }

        // first read header
        std::size_t n = 0;
        while (n != 8)
        {
            n += co_await _socket.async_read_some(asio::buffer(header_data + n, 8 - n), use_awaitable);
        }

        int msg_len = util::chars_to_int32_big_endian(header_data);
        int msg_type = util::chars_to_int32_big_endian(header_data + 4);

        // allocate bytes to save incoming data
        v_data.resize(msg_len);

        n = 0;
        while (n != msg_len)
        {
            n += co_await _socket.async_read_some(asio::buffer(v_data.data() + n, msg_len - n), use_awaitable);
        }

        auto resp = router_.route(v_data.data(), n, static_cast<MessageType>(msg_type), _id);
        n = 0;
        while (n != resp.size())
        {
            n += co_await async_write(_socket, asio::buffer(resp.data() + n, resp.size() - n), use_awaitable);
        }
    }
}

std::string Peer::ID()
{
    return _id;
}

void Peer::update_addr_info(std::string &new_addr, int new_port)
{
    // the only info that needs to be updated when the peer addr info changes is the ID (for now)
    std::lock_guard<std::mutex> lg(_state_mu);
    _id = util::addr_and_port_to_str(new_addr, new_port);
    l_addr = new_addr;
    l_port = new_port;
}

// NOTE: check this when app is more mulithreaded
void Peer::pause_async_read()
{
    {
        std::lock_guard<std::mutex> lock(_async_cv_mu);
        _sync_is_running = true;
    }
    // cancel all async operations for this socket, to read synchronously now
    _socket.cancel();
}

void Peer::resume_async_read()
{
    {
        std::lock_guard<std::mutex> lock(_async_cv_mu);
        _sync_is_running = false;
    }
    _async_cv.notify_one();
}

std::vector<unsigned char> Peer::send(std::vector<unsigned char> &data, bool expecting_resp)
{
    // TODO: figure if this one could be a problem because the async_read will lock too
    std::lock_guard<std::mutex> lg(_socket_mu);

    pause_async_read();

    // data is assumed to be in the format below:
    // 4 bytes of message size
    // 4 bytes of message type
    // x bytes of marshalled protobuf message

    std::size_t n = 0;
    std::vector<unsigned char> resp;

    // send data to peer
    while (n != data.size())
    {
        n += _socket.write_some(asio::buffer(data.data() + n, data.size() - n));
    }

    if (!expecting_resp)
    {
        // caller is not waiting for response
        resume_async_read();
        std::cout << "not expecting resp, returning..." << std::endl;
        return resp;
    }

    // now read response synchronously
    n = 0;
    unsigned char header_data[8];
    while (n != 8)
    {
        n += _socket.read_some(asio::buffer(header_data + n, 8 - n));
    }

    int msg_len = util::chars_to_int32_big_endian(header_data);
    int msg_type = util::chars_to_int32_big_endian(header_data + 4);

    // allocate bytes to save incoming data
    resp.resize(msg_len);

    n = 0;
    while (n != msg_len)
    {
        n += _socket.read_some(asio::buffer(resp.data() + n, msg_len - n));
    }

    resume_async_read();
    return resp;
}

awaitable<void> Peer::async_send(std::shared_ptr<std::vector<unsigned char>> data_ptr)
{
    auto data = *data_ptr;

    std::lock_guard<std::mutex> lg(_socket_mu);


    std::size_t n = 0;
    while (n != data.size())
    {
        n += co_await async_write(_socket, asio::buffer(data.data() + n, data.size() - n), use_awaitable);
    }
}

std::pair<std::string, int> Peer::get_real_addr()
{
    return std::make_pair<std::string, int>(_socket.remote_endpoint().address().to_string(), _socket.remote_endpoint().port());
}

std::pair<std::string, int> Peer::get_listening_addr()
{
    // if l_addr has not been set, it means that the listening address is equal to the real address
    if (l_addr.size() == 0)
    {
        return get_real_addr();
    }

    return std::make_pair(l_addr, l_port);
}