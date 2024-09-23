#ifndef DIPLO_MEM_BLOCK_STORE_HPP
#define DIPLO_MEM_BLOCK_STORE_HPP

#include "store/interface/i_blockstore.hpp"

#include <thread>
#include <memory>
#include <vector>

#include <unordered_map>

class MemBlockStore : public IBlockStore
{
public:
    MemBlockStore() = default;

    bool store_block(const std::vector<unsigned char> &block_hash, std::shared_ptr<Block> block) override;
    std::shared_ptr<Block> get_block(const std::vector<unsigned char> &block_hash) override;
    bool remove_block(const std::vector<unsigned char> &block_hash) override;
    bool exists(const std::vector<unsigned char> &block_hash) override;

private:
    std::mutex mu_;
    std::unordered_map<std::string, std::shared_ptr<Block>> storage_;

    std::string blockhash_to_key(const std::vector<unsigned char> &block_hash);
};

#endif