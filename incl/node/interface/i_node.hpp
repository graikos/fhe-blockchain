#ifndef DIPLO_I_NODE_HPP
#define DIPLO_I_NODE_HPP

#include <utility>
#include <vector>
#include <string>

#include "message.pb.h"

#include "nlohmann/json.hpp"

using json = nlohmann::json;

constexpr int STATUS_OK = 200;
constexpr int STATUS_BAD_REQUEST = 400;
constexpr int STATUS_PAYMENT_REQUIRED = 402;
constexpr int STATUS_NOT_FOUND = 404;
constexpr int STATUS_INTERNAL_SERVER_ERROR = 500;

class INode
{
public:
    INode() = default;

    virtual std::vector<unsigned char> handle_get_peer_addrs(GetAddr &, const std::string &) = 0;
    virtual void handle_addr(Addr &) = 0;

    virtual void rpc_handle_say_hello() = 0;
    virtual void rpc_handle_transaction(const json &req, json &resp) = 0;
    virtual void rpc_handle_computation(const json &req, json &resp) = 0;
    virtual void rpc_handle_output(const json &req, json &resp) = 0;

    virtual std::vector<unsigned char> handle_inv_block(const InvBlock &msg) = 0;
    virtual std::vector<unsigned char> handle_get_block(const GetBlock &msg) = 0;
    virtual std::vector<unsigned char> handle_info_block(const InfoBlock &msg) = 0;
    virtual std::vector<unsigned char> handle_info_block_sync(const InfoBlock &msg) = 0;

    virtual std::vector<unsigned char> handle_inv_tx(const InvTransaction &msg) = 0;
    virtual std::vector<unsigned char> handle_get_tx(const GetTransaction &msg) = 0;
    virtual std::vector<unsigned char> handle_info_tx(const InfoTransaction &msg) = 0;

    virtual std::vector<unsigned char> handle_inv_computation(const InvComputation &msg) = 0;
    virtual std::vector<unsigned char> handle_get_computation(const GetComputation &msg) = 0;
    virtual std::vector<unsigned char> handle_info_computation(const InfoComputation &msg) = 0;

    virtual std::vector<unsigned char> handle_sync_block(const SyncBlock &msg) = 0;

    virtual std::vector<unsigned char> handle_sync_transactions(const SyncTransactions &msg) = 0;
    virtual std::vector<unsigned char> handle_list_transactions(const ListTransactions &msg) = 0;

    virtual std::vector<unsigned char> build_invalid() = 0;

    virtual bool is_synced() = 0;
    virtual void set_synced() = 0;
};
#endif