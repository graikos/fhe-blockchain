#include "net/conn_mngr.hpp"
#include "util/util.hpp"
#include "net/router.hpp"
#include <iostream>
#include <thread>

using json = nlohmann::json;

ConnectionManager::ConnectionManager(Router &router, asio::io_context &ctx, json &config) : router_(router), io_context(ctx), config_(config)
{
    outbound_peers_lim_ = config_["net"]["outbound_peers_limit"];
    inbound_peers_lim_ = config_["net"]["inbound_peers_limit"];

    listening_address = config_["net"]["address"];
    listening_port = config_["net"]["port"];
}

void ConnectionManager::setup()
{
    co_spawn(io_context, listener(), detached);
}

awaitable<void> ConnectionManager::listener()
{

    std::cout << "started listening on: " << config_["net"]["address"] << ":" << config_["net"]["port"] << std::endl;
    auto executor = co_await this_coro::executor;
    tcp::acceptor acceptor(executor, {asio::ip::address::from_string(config_["net"]["address"]), config_["net"]["port"]});

    for (;;)
    {
        tcp::socket socket = co_await acceptor.async_accept(use_awaitable);

        // creating new peer object for this connection
        auto new_peer = std::make_shared<Peer>(router_, std::move(socket), io_context);

        // add to peer map
        add_to_inbound(new_peer);

        // DEBUG
        print_inbound();

        co_spawn(executor, new_peer->listen(), [this, new_peer](std::exception_ptr e)
                 {
            try
            {
                std::rethrow_exception(e);
            } catch (std::exception &exc)
            {
                // remove peer from inbound connections when the listen fails
                this->remove_peer_from_inbound(new_peer->ID());
                // DEBUG
                print_inbound();
            } });
    }
}

std::vector<std::pair<std::string, int>> ConnectionManager::collect_peers()
{
    std::lock_guard<std::mutex> inlg(inb_mu);
    std::lock_guard<std::mutex> outlg(outb_mu);
    std::vector<std::pair<std::string, int>> res;
    for (const auto &p : inbound_peer_map_)
    {
        res.push_back(p.second->get_listening_addr());
    }
    for (const auto &p : outbound_peer_map_)
    {
        res.push_back(p.second->get_listening_addr());
    }
    return res;
};

bool ConnectionManager::outbound_full()
{
    return outbound_peer_map_.size() == outbound_peers_lim_;
}

bool ConnectionManager::inbound_full()
{
    return inbound_peer_map_.size() == inbound_peers_lim_;
}

bool ConnectionManager::peer_exists_outbound(const std::string &peerID)
{
    std::lock_guard<std::mutex> outlg(outb_mu);
    auto it = outbound_peer_map_.find(peerID);
    return (it != outbound_peer_map_.end());
}

bool ConnectionManager::peer_exists_inbound(const std::string &peerID)
{
    std::lock_guard<std::mutex> inlg(inb_mu);
    auto it = inbound_peer_map_.find(peerID);
    return (it != inbound_peer_map_.end());
}

bool ConnectionManager::peer_exists(const std::string &address, int port)
{
    auto id = util::addr_and_port_to_str(address, port);
    return peer_exists_inbound(id) || peer_exists_outbound(id);
}

void ConnectionManager::update_inb_peer_addr_info(const std::string peerID, std::string &new_addr, int new_port)
{
    std::lock_guard<std::mutex> inlg(inb_mu);
    auto peer = inbound_peer_map_[peerID];
    peer->update_addr_info(new_addr, new_port);
    // adding entry under new ID
    inbound_peer_map_[peer->ID()] = peer;
    // remove old entry, under peerID passed as arg
    if (peerID != peer->ID())
    {
        inbound_peer_map_.erase(peerID);
    }
}

std::vector<unsigned char> ConnectionManager::send_to_peer(const std::string &addr, int port, std::vector<unsigned char> &data, bool expecting_resp)
{

    auto peerID = util::addr_and_port_to_str(addr, port);

    std::shared_ptr<Peer> peer;

    if (peer_exists_inbound(peerID))
    {
        std::lock_guard<std::mutex> lg(inb_mu);
        peer = inbound_peer_map_[peerID];
    }
    else if (peer_exists_outbound(peerID))
    {
        std::lock_guard<std::mutex> lg(outb_mu);
        peer = outbound_peer_map_[peerID];
    }
    else
    {
        // peer not found, connect and get from outbound connections
        connect_to_peer(addr, port);
        std::lock_guard<std::mutex> lg(outb_mu);
        peer = outbound_peer_map_[peerID];
    }

    std::vector<unsigned char> resp;
    resp = peer->send(data, expecting_resp);
    return resp;
}

void ConnectionManager::async_send_to_peer(const std::string &addr, int port, std::vector<unsigned char> &data)
{

    auto peerID = util::addr_and_port_to_str(addr, port);

    std::shared_ptr<Peer> peer;

    if (peer_exists_inbound(peerID))
    {
        std::lock_guard<std::mutex> lg(inb_mu);
        peer = inbound_peer_map_[peerID];
    }
    else if (peer_exists_outbound(peerID))
    {
        std::lock_guard<std::mutex> lg(outb_mu);
        peer = outbound_peer_map_[peerID];
    }
    else
    {
        std::cout << "async send peer not found, will try to connect anew" << std::endl;
        // peer not found, connect and get from outbound connections
        connect_to_peer(addr, port);
        std::lock_guard<std::mutex> lg(outb_mu);
        peer = outbound_peer_map_[peerID];
        std::cout << "async send at end of the 'else'" << std::endl;
    }

    async_send_to_peer(peer, data);
}

void ConnectionManager::async_send_to_peer(std::shared_ptr<Peer> peer, std::vector<unsigned char> &data)
{
    co_spawn(io_context, peer->async_send(std::make_shared<std::vector<unsigned char>>(data)), detached);
}

// connect_to_peer establishes an outbound connection to the specified endpoint
// NOTE: does not check if the peer already exists
void ConnectionManager::connect_to_peer(const std::string &addr, int port)
{
    auto peer_id = util::addr_and_port_to_str(addr, port);

    asio::ip::address a = asio::ip::address::from_string(addr);
    tcp::endpoint ep(a, port);

    // make new connection to peer
    tcp::socket socket(io_context);
    socket.connect(ep);

    // add to outbound connections
    auto new_peer = std::make_shared<Peer>(router_, std::move(socket), io_context);
    add_to_outbound(new_peer);

    // start monitoring new connection
    co_spawn(io_context, new_peer->listen(), detached);

    return;
}

void ConnectionManager::add_to_outbound(std::shared_ptr<Peer> peer)
{
    std::lock_guard<std::mutex> lg(outb_mu);
    if (outbound_full())
    {
        return;
    }
    outbound_peer_map_[peer->ID()] = peer;
}

void ConnectionManager::add_to_inbound(std::shared_ptr<Peer> peer)
{
    std::lock_guard<std::mutex> lg(inb_mu);
    if (inbound_full())
    {
        return;
    }
    inbound_peer_map_[peer->ID()] = peer;
}

void ConnectionManager::remove_peer_from_inbound(const std::string &peerID)
{
    std::lock_guard<std::mutex> lg(inb_mu);
    inbound_peer_map_.erase(peerID);
}

void ConnectionManager::print_inbound()
{
    std::lock_guard<std::mutex> lg(inb_mu);
    std::cout << "==================================" << std::endl;
    std::cout << "Inbound peer count: " << inbound_peer_map_.size() << std::endl;
    for (const auto &p : inbound_peer_map_)
    {
        std::cout << p.first << std::endl;
    }
    std::cout << "==================================" << std::endl;
}

void ConnectionManager::print_outbound()
{
    std::lock_guard<std::mutex> lg(outb_mu);
    std::cout << "==================================" << std::endl;
    std::cout << "Outbound peer count: " << outbound_peer_map_.size() << std::endl;
    for (const auto &p : outbound_peer_map_)
    {
        std::cout << p.first << std::endl;
    }
    std::cout << "==================================" << std::endl;
}

std::shared_ptr<Peer> ConnectionManager::get_one_outbound()
{
    std::lock_guard<std::mutex> lg(outb_mu);
    for (const auto &p : outbound_peer_map_)
    {
        return p.second;
    }
    throw std::out_of_range("no outbound peers");
}

void ConnectionManager::async_broadcast(const std::vector<unsigned char> &data)
{
    std::lock_guard<std::mutex> lgi(inb_mu);
    for (const auto &p : inbound_peer_map_)
    {
        co_spawn(io_context, p.second->async_send(std::make_shared<std::vector<unsigned char>>(data)), detached);
    }

    std::lock_guard<std::mutex> lgo(outb_mu);
    for (const auto &p : outbound_peer_map_)
    {
        co_spawn(io_context, p.second->async_send(std::make_shared<std::vector<unsigned char>>(data)), detached);
    }
}