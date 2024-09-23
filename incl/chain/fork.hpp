#ifndef DIPLO_FORK_HPP
#define DIPLO_FORK_HPP

#include "core/block_header.hpp"
#include "core/block.hpp"
#include "chain/chain.hpp"
#include "store/interface/i_blockstore.hpp"
#include "store/interface/i_chainstate.hpp"
#include "store/interface/i_compstore.hpp"

#include <cstdint>
#include <vector>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Fork should be managed from chain manager, so no concurrency control
class Fork : public Chain
{
public:
    uint32_t chain_src_;
    std::shared_ptr<BlockHeader> chain_src_header_;
    Fork(const json &config, std::shared_ptr<IChainstate> chainstate, std::shared_ptr<IBlockStore> block_store, std::shared_ptr<ICompStore> comp_store,
         uint32_t chain_src, std::shared_ptr<BlockHeader> chain_src_header, uint64_t diff);

    uint32_t current_fork_height();
    bool append_block(std::shared_ptr<Block> block);

private:
    std::mutex fork_mu_;
    const json &config_;
};

#endif