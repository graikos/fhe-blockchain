#ifndef DIPLO_NODE_HPP
#define DIPLO_NODE_HPP

#include <string>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <thread>

#include "node/interface/i_node.hpp"
#include "net/conn_mngr.hpp"
#include "net/router.hpp"

#include "net/rpc_server.hpp"

#include <nlohmann/json.hpp>

#include "chain/chain_manager.hpp"
#include "computer/fhe_computer.hpp"

#include "store/mem_blockstore.hpp"
#include "store/mem_chainstate.hpp"
#include "store/mem_compstore.hpp"
#include "store/mem_pool.hpp"

#include "wallet/wallet.hpp"

using json = nlohmann::json;

class Node : public INode
{
public:
    Node() = default;
    Node(json &config, asio::io_context &io_context, std::shared_ptr<IChainstate> cs, std::shared_ptr<IBlockStore> bs,
         std::shared_ptr<IMemPool> mp, std::shared_ptr<ICompStore> compstore);

    void start();

    std::vector<unsigned char> build_getaddr();
    std::vector<unsigned char> build_hello();
    std::vector<unsigned char> build_invalid() override;
    std::vector<unsigned char> build_inv_block(const std::vector<unsigned char> &block_hash);
    std::vector<unsigned char> build_inv_tx(const std::vector<unsigned char> &txid);
    std::vector<unsigned char> build_inv_computation(const std::vector<unsigned char> &comp_hash);
    std::vector<unsigned char> build_sync_block(uint32_t height);
    std::vector<unsigned char> build_get_tx(const std::vector<unsigned char> &txid);
    std::vector<unsigned char> build_sync_transactions();

    std::vector<unsigned char> handle_get_peer_addrs(GetAddr &ga, const std::string &peerID) override;
    void handle_addr(Addr &addrVec) override;

    void rpc_handle_say_hello() override;
    void rpc_handle_transaction(const json &msg, json &resp) override;
    void rpc_handle_computation(const json &req, json &resp) override;
    void rpc_handle_output(const json &req, json &resp) override;

    void handle_add_valid_block(std::shared_ptr<Block> block);

    std::vector<unsigned char> handle_inv_block(const InvBlock &msg) override;
    std::vector<unsigned char> handle_get_block(const GetBlock &msg) override;
    std::vector<unsigned char> handle_info_block(const InfoBlock &msg) override;
    std::vector<unsigned char> handle_info_block_sync(const InfoBlock &msg) override;

    std::vector<unsigned char> handle_inv_tx(const InvTransaction &msg) override;
    std::vector<unsigned char> handle_get_tx(const GetTransaction &msg) override;
    std::vector<unsigned char> handle_info_tx(const InfoTransaction &msg) override;

    std::vector<unsigned char> handle_inv_computation(const InvComputation &msg) override;
    std::vector<unsigned char> handle_get_computation(const GetComputation &msg) override;
    std::vector<unsigned char> handle_info_computation(const InfoComputation &msg) override;

    std::vector<unsigned char> handle_sync_block(const SyncBlock &msg) override;

    std::vector<unsigned char> handle_sync_transactions(const SyncTransactions &msg) override;
    std::vector<unsigned char> handle_list_transactions(const ListTransactions &msg) override;

    void handle_mined_block(std::shared_ptr<Block> mined_block);

    bool is_synced() override;
    void set_synced() override;

private:
    asio::io_context &io_context_;

    void bootstrap_from_config(const json &config);
    void start_sync();
    void sync_mempool();

    std::shared_ptr<std::atomic<bool>> stop_flag_;

    std::unique_ptr<Router> router_;
    std::unique_ptr<RPCRouter> rpc_router_;

    std::unique_ptr<ConnectionManager> conn_manager_;
    std::unique_ptr<RPCServer> rpc_server_;

    std::unique_ptr<ChainManager> chain_manager_;

    std::shared_ptr<Wallet> wallet_;

    std::mutex sync_mu_;
    std::condition_variable sync_cv_;
    bool is_synced_;
    std::shared_ptr<Peer> sync_peer_;
};

#endif
