#ifndef DIPLO_CHAIN_MANAGER_HPP
#define DIPLO_CHAIN_MANAGER_HPP

#include <memory>
#include <vector>
#include <thread>
#include <atomic>

#include "nlohmann/json.hpp"

#include "chain/chain.hpp"
#include "chain/fork.hpp"
#include "chain/miner.hpp"

#include "wallet/wallet.hpp"

#include "store/interface/i_blockstore.hpp"
#include "store/interface/i_chainstate.hpp"
#include "store/interface/i_mempool.hpp"
#include "store/interface/i_compstore.hpp"
#include "core/interface/computation.hpp"

using json = nlohmann::json;

class ChainManager
{
private:
    const json &config_;
    std::mutex chain_manager_mu_;

    void reorg(std::shared_ptr<Fork> fork);

protected:
    std::shared_ptr<IChainstate> chainstate_;
    std::shared_ptr<IBlockStore> block_store_;
    std::shared_ptr<IMemPool> mem_pool_;
    std::shared_ptr<ICompStore> comp_store_;

    std::unique_ptr<Miner> miner_;

    std::shared_ptr<Wallet> wallet_;

public:
    std::unique_ptr<Chain> main_chain_;
    std::vector<std::shared_ptr<Fork>> forks_;

    ChainManager(const json &config, std::shared_ptr<IChainstate> chainstate, std::shared_ptr<IBlockStore> blockstore,
                 std::shared_ptr<IMemPool> mem_pool, std::shared_ptr<ICompStore> comp_store,
                 std::shared_ptr<std::atomic<bool>> stop_flag, std::shared_ptr<Wallet> wallet);

    bool add_block(std::shared_ptr<Block> block, bool is_main_and_valid = false);

    void start_mining();
    bool have_mined_block();
    std::shared_ptr<Block> get_mined_block();
    bool block_exists(const std::vector<unsigned char> &block_hash);
    std::shared_ptr<Block> get_block(const std::vector<unsigned char> &block_hash);

    bool tx_exists(const std::vector<unsigned char> &txid);
    std::shared_ptr<Transaction> get_tx(const std::vector<unsigned char> &txid);
    bool add_tx(std::shared_ptr<Transaction> tx);

    void set_wallet(std::shared_ptr<Wallet> wallet);

    bool add_computation(std::shared_ptr<Computation> comp);
    bool computation_exists(const std::vector<unsigned char> &comp_hash);
    std::shared_ptr<Computation> get_computation(const std::vector<unsigned char> &comp_hash);

    std::vector<std::string> mempool_list_txid();
    std::vector<std::string> compstore_list_comp_hashes();

};

#endif