#include "chain/fork.hpp"
#include "util/util.hpp"
#include <iostream>

Fork::Fork(const json &config, std::shared_ptr<IChainstate> chainstate, std::shared_ptr<IBlockStore> block_store,std::shared_ptr<ICompStore> comp_store, uint32_t chain_src, std::shared_ptr<BlockHeader> chain_src_header, uint64_t diff)
    : Chain(config, chainstate, block_store, comp_store, true), config_(config), chain_src_(chain_src), chain_src_header_(chain_src_header)
{
    total_difficulty_ = diff;
}

uint32_t Fork::current_fork_height()
{
    // no -1 here, because fork assumes at least one block already in main chain
    return chain_src_ + header_chain_.size();
}

bool Fork::append_block(std::shared_ptr<Block> block)
{
    auto new_height = current_fork_height() + 1;

    std::shared_ptr<BlockHeader> head = chain_src_header_;
    if (header_chain_.size() > 0)
    {
        head = header_chain_.back();
    }

    // check if block can be attached
    if (!can_attach(block->header_))
    {
        std::cout << "Cannot attach block to this fork." << std::endl;
        throw std::invalid_argument("Could not attach block to this fork.");
    }

    if (!validate_header_unsafe(block->header_, new_height))
    {
        std::cout << "Invalid block header for fork." << std::endl;
        return false;
    }

    // block is valid, add to chain
    block->header_->prev_block_header_ = head;
    header_chain_.push_back(block->header_);

    // even though this is a fork, we keep the block in store to retrieve info later and validate
    block_store_->store_block(block->hash(), block);

    total_difficulty_ += block->header_->difficulty_;

    return true;
}
