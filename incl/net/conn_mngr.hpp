#ifndef DIPLO_CONN_MNGR_HPP
#define DIPLO_CONN_MNGR_HPP

#include <memory>
#include <unordered_map>
#include <utility>
#include <thread>
#include "net/router.hpp"
#include "net/peer.hpp"

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

class ConnectionManager
{
public:
    ConnectionManager() = default;
    ConnectionManager(Router &router, asio::io_context &ctx, json &config);

    void setup();

    std::vector<std::pair<std::string, int>> collect_peers();

    std::vector<unsigned char> send_to_peer(const std::string &addr, int port, std::vector<unsigned char> &data, bool expecting_resp);
    void async_send_to_peer(const std::string &addr, int port, std::vector<unsigned char> &data);
    void async_send_to_peer(std::shared_ptr<Peer> peer, std::vector<unsigned char> &data);
    void connect_to_peer(const std::string &addr, int port);

    bool outbound_full();
    bool inbound_full();

    bool peer_exists(const std::string &address, int port);

    void async_broadcast(const std::vector<unsigned char> &data);

    awaitable<void> listener();

    std::shared_ptr<Peer> get_one_outbound();

    // DEBUG:
    void print_inbound();
    void print_outbound();

    void update_inb_peer_addr_info(const std::string peerID, std::string &new_addr, int new_port);

    std::string listening_address;
    int listening_port;

private:
    Router &router_;

    std::mutex inb_mu;
    std::unordered_map<std::string, std::shared_ptr<Peer>> inbound_peer_map_;

    std::mutex outb_mu;
    std::unordered_map<std::string, std::shared_ptr<Peer>> outbound_peer_map_;

    asio::io_context &io_context;

    bool peer_exists_outbound(const std::string &peerID);
    bool peer_exists_inbound(const std::string &peerID);

    int outbound_peers_lim_;
    int inbound_peers_lim_;

    void remove_peer_from_inbound(const std::string &peerID);

    void add_to_outbound(std::shared_ptr<Peer> peer);
    void add_to_inbound(std::shared_ptr<Peer> peer);


    json config_;
};

#endif