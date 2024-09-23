#ifndef DIPLO_CHAIN_HPP
#define DIPLO_CHAIN_HPP

#include <nlohmann/json.hpp>

#include "core/block_header.hpp"
#include "store/interface/i_mempool.hpp"
#include "store/interface/i_chainstate.hpp"
#include "store/interface/i_blockstore.hpp"
#include "store/interface/i_compstore.hpp"

#include <memory>
#include <cstdint>
#include <thread>

using json = nlohmann::json;

class Chain
{
private:
    const json &config_;
    std::mutex chain_mu_;

    std::shared_ptr<Block> create_genesis();

public:
    Chain(const json &config, std::shared_ptr<IChainstate> chainstate, std::shared_ptr<IBlockStore> block_store, std::shared_ptr<IMemPool> mem_pool, std::shared_ptr<ICompStore> comp_store);
    Chain(const json &config, std::shared_ptr<IChainstate> chainstate, std::shared_ptr<IBlockStore> block_store, std::shared_ptr<ICompStore> comp_store, bool is_fork);

    std::shared_ptr<BlockHeader> head_header();
    std::vector<std::shared_ptr<BlockHeader>> header_chain_;
    uint64_t total_difficulty_;

    virtual bool append_block(std::shared_ptr<Block> block, bool is_already_valid = false);
    bool can_attach(std::shared_ptr<BlockHeader> header);
    bool validate_block(std::shared_ptr<Block> block, uint32_t height);
    bool validate_header_unsafe(std::shared_ptr<BlockHeader> header, uint32_t height);
    uint32_t current_height();
    uint64_t size();
    std::shared_ptr<BlockHeader> get_header(uint32_t idx);

    uint32_t get_current_epoch();
    uint32_t get_epoch(uint32_t height);
    uint64_t reward_for_height(uint32_t height_to_check);
    uint32_t get_difficulty_for_height(uint32_t height_to_check);

    // DEBUG
    void print_chain_hashes_force();

protected:
    std::shared_ptr<IChainstate> chainstate_;
    std::shared_ptr<IBlockStore> block_store_;
    std::shared_ptr<IMemPool> mem_pool_;
    std::shared_ptr<ICompStore> comp_store_;
};

#endif