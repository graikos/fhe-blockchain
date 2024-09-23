#ifndef DIPLO_MINER_HPP
#define DIPLO_MINER_HPP

#include "store/interface/i_mempool.hpp"
#include "store/interface/i_compstore.hpp"
#include "core/block.hpp"
#include "core/block_header.hpp"
#include "core/transaction.hpp"

#include "wallet/wallet.hpp"

#include <memory>
#include <atomic>

class Miner
{
public:
    bool have_result_;
    std::shared_ptr<Block> result;
    Miner(std::shared_ptr<std::atomic<bool>> stop_flag, std::shared_ptr<IMemPool> mem_pool, std::shared_ptr<ICompStore> comp_store);

    void mine(std::shared_ptr<BlockHeader> prev_header, uint32_t height, uint32_t difficutly, uint64_t reward,
              const std::vector<std::shared_ptr<Transaction>> &tx, const std::vector<std::shared_ptr<Computation>> &comps,
              std::shared_ptr<Wallet> wallet);
    bool have_result();

private:
    std::shared_ptr<std::atomic<bool>> stop_flag_;
    std::shared_ptr<IMemPool> mem_pool_;
    std::shared_ptr<ICompStore> comp_store_;
};

#endif