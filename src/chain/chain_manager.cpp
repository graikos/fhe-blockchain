#include "chain/chain_manager.hpp"
#include <thread>
#include <iostream>

#include "util/util.hpp"

ChainManager::ChainManager(const json &config, std::shared_ptr<IChainstate> chainstate, std::shared_ptr<IBlockStore> blockstore,
                           std::shared_ptr<IMemPool> mem_pool, std::shared_ptr<ICompStore> comp_store,
                           std::shared_ptr<std::atomic<bool>> stop_flag, std::shared_ptr<Wallet> wallet)
    : config_(config), chainstate_(chainstate), block_store_(blockstore), mem_pool_(mem_pool),
      comp_store_(comp_store), miner_(std::make_unique<Miner>(stop_flag, mem_pool, comp_store)), wallet_(wallet),
      main_chain_(std::make_unique<Chain>(config, chainstate, blockstore, mem_pool, comp_store))
{
}

bool ChainManager::add_block(std::shared_ptr<Block> block, bool is_main_and_valid)
{
    std::lock_guard<std::mutex> lg(chain_manager_mu_);

    if (is_main_and_valid)
    {
        bool added = main_chain_->append_block(block, true);
        if (added)
        {
            wallet_->filter_block(block);
            wallet_->spend_block(block);
        }
        return added;
    }

    if (main_chain_->can_attach(block->header_))
    {
        bool added = main_chain_->append_block(block);
        if (added)
        {
            wallet_->filter_block(block);
            wallet_->spend_block(block);
        }
        return added;
    }

    std::cout << "check for fork" << std::endl;
    // could not attach block to main chain. Find possible attachment point:
    bool found_fork = false;
    for (auto &fork : forks_)
    {
        if (fork->can_attach(block->header_))
        {
            found_fork = true;
            if (!fork->append_block(block))
            {
                // found attachment point, but invalid block header
                return false;
            }

            if (fork->total_difficulty_ > main_chain_->total_difficulty_)
            {
                reorg(fork);
                return true;
            }

            break;
        }
    }

    if (found_fork)
    {
        // if it reaches this point, fork was found, block was appended, no reorg needed
        return true;
    }

    uint64_t total_diff = 0;
    // here, means no fork was found, linear search all chain
    for (uint64_t i = 0; i < main_chain_->size(); ++i)
    {
        total_diff += main_chain_->header_chain_[i]->difficulty_;
        if (block->header_->prev_hash() == main_chain_->header_chain_[i]->hash())
        {
            // found new point
            auto new_fork = std::make_shared<Fork>(config_, chainstate_, block_store_, comp_store_, i, main_chain_->header_chain_[i], total_diff);
            if (!new_fork->append_block(block))
            {
                // found attachment point for new fork, but block header was invalid
                return false;
            }
            forks_.push_back(new_fork);
            return true;
        }
    }

    // no attachment point found
    return false;
}

void ChainManager::reorg(std::shared_ptr<Fork> fork)
{
    // already locked, re-org is called from append

    auto old_main_fork = std::make_shared<Fork>(config_, chainstate_, block_store_, comp_store_, fork->chain_src_, main_chain_->header_chain_[fork->chain_src_], main_chain_->total_difficulty_);
    for (uint64_t i = fork->chain_src_ + 1; i < main_chain_->size(); ++i)
    {
        // directly insert in chain, since we know everything else is valid with this chain
        old_main_fork->header_chain_.push_back(main_chain_->header_chain_[i]);
    }

    // rewind chainstate to the point where the fork happens
    for (uint64_t i = main_chain_->size() - 1; i > fork->chain_src_; --i)
    {
        // first retrieve block
        auto block = block_store_->get_block(main_chain_->header_chain_[i]->hash());
        chainstate_->rewind_block(block);
        // NOTE: transactions that were part of the re org will be lost and not be in the mem pool
        // rewind is not straightforward for mempool, since only valid tx should be in it
    }

    uint64_t old_main_total_diff = main_chain_->total_difficulty_;

    // now resize main chain to start appending fork blocks
    // keep all blocks till the fork point
    main_chain_->header_chain_.resize(fork->chain_src_ + 1);
    bool invalid_found = false;
    uint64_t idx = 0;
    for (auto &h : fork->header_chain_)
    {
        auto block = block_store_->get_block(h->hash());
        // NOTE: here total_diff of main chain will increase and will be wrong, until it's fixed again in the end
        if (!main_chain_->append_block(block))
        {
            invalid_found = true;
            break;
        }
        ++idx;
    }

    if (invalid_found)
    {
        // during full validation of fork, an invalid block was found
        // meaing, once again, rewind to src point
        for (uint64_t i = main_chain_->size() - 1; i > fork->chain_src_; --i)
        {
            auto block = block_store_->get_block(main_chain_->header_chain_[i]->hash());
            chainstate_->rewind_block(block);
        }

        // resize again to start appending
        main_chain_->header_chain_.resize(fork->chain_src_ + 1);
        for (auto &mtx : old_main_fork->header_chain_)
        {
            auto block = block_store_->get_block(mtx->hash());
            assert(main_chain_->append_block(block));
        }

        for (uint64_t i = fork->header_chain_.size() - 1; i >= idx; --i)
        {
            // remove invalid part difficulty
            fork->total_difficulty_ -= fork->header_chain_[i]->difficulty_;
        }
        // trim fork to remove invalid part
        fork->header_chain_.resize(idx);
        main_chain_->total_difficulty_ = old_main_total_diff;
    }
    else
    {
        // main chain was replaced
        // remove old fork from fork vector
        // replace main difficulty with forkk
        auto it = forks_.begin();
        bool f = false;
        for (; it != forks_.end(); ++it)
        {
            if ((*it) == fork)
            {
                f = true;
                break;
            }
        }
        assert(f);
        forks_.erase(it);

        main_chain_->total_difficulty_ = fork->total_difficulty_;

        forks_.push_back(old_main_fork);
    }

    // this is a very naive and inefficient way, but does the job in this case
    wallet_->rescan(main_chain_->header_chain_, block_store_);
}

void ChainManager::start_mining()
{
    // reset miner state
    // NOTE: stop flag must be reset accordingly by the caller
    miner_->have_result_ = false;
    miner_->result = std::shared_ptr<Block>(nullptr);

    // collect mem_pool transations, this is a soft upper limit, more like a default setting
    auto tx_to_mine = mem_pool_->get_top(config_.at("chain").at("default_tx_per_block"));
    if (tx_to_mine.empty())
    {
        std::cout << "No TXs to add to block being mined..." << std::endl;
    }

    auto diff_target = main_chain_->get_difficulty_for_height(main_chain_->current_height() + 1);
    auto reward = main_chain_->reward_for_height(main_chain_->current_height() + 1);

    // target to reach will be the next height of the main chain
    auto comps_to_use = comp_store_->collect_computations(diff_target);

    if (comps_to_use.size() == 0)
    {
        // NOTE: the correct way is this to be a condition variable, but for this purpose sleep will work
        // it's just busy wait
        std::cout << "No comps to use, sleeping..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return;
    }

    miner_->mine(main_chain_->head_header(), main_chain_->current_height() + 1, diff_target, reward,
                 tx_to_mine, comps_to_use, wallet_);
}

bool ChainManager::have_mined_block()
{
    return miner_->have_result();
}

std::shared_ptr<Block> ChainManager::get_mined_block()
{
    return miner_->result;
}

bool ChainManager::block_exists(const std::vector<unsigned char> &block_hash)
{
    return block_store_->exists(block_hash);
}

std::shared_ptr<Block> ChainManager::get_block(const std::vector<unsigned char> &block_hash)
{
    return block_store_->get_block(block_hash);
}

bool ChainManager::tx_exists(const std::vector<unsigned char> &txid)
{
    return mem_pool_->exists(txid);
}

std::shared_ptr<Transaction> ChainManager::get_tx(const std::vector<unsigned char> &txid)
{
    return mem_pool_->get_tx(txid);
}

// should only be used for non-coinbase transactions
bool ChainManager::add_tx(std::shared_ptr<Transaction> tx)
{
    std::vector<std::vector<unsigned char>> pubkeys;
    // here apart from regular validations of signature, we need to check if UTXOs are in chainstate
    for (const auto &inp : tx->inputs_)
    {
        if (!chainstate_->exists(inp->TXID_, inp->vout_))
        {
            return false;
        }
        pubkeys.push_back(chainstate_->pubkey(inp->TXID_, inp->vout_));
        // set input amount to validate fees later
        inp->set_amount(chainstate_->amount(inp->TXID_, inp->vout_));
    }

    if (!tx->validate_transaction(pubkeys))
    {
        return false;
    }

    // at this point, we've made sure the UTXOs referenced are indeed in UTXOs
    // and the signatures provided are valid, as well as amount etc.
    return mem_pool_->add_valid_tx(tx);
}

void ChainManager::set_wallet(std::shared_ptr<Wallet> wallet)
{
    wallet_ = wallet;
}

bool ChainManager::add_computation(std::shared_ptr<Computation> comp)
{
    return comp_store_->store_computation(comp);
}

bool ChainManager::computation_exists(const std::vector<unsigned char> &comp_hash)
{
    return comp_store_->exists(comp_hash);
}

std::shared_ptr<Computation> ChainManager::get_computation(const std::vector<unsigned char> &comp_hash)
{
    return comp_store_->get_computation(comp_hash);
}

std::vector<std::string> ChainManager::mempool_list_txid()
{
    return mem_pool_->list_txids();
}

std::vector<std::string> ChainManager::compstore_list_comp_hashes()
{
    return comp_store_->list_comp_hashes();
}