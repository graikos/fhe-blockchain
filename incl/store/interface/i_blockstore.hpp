#ifndef DIPLO_BLOCK_STORE_HPP
#define DIPLO_BLOCK_STORE_HPP

#include <vector>
#include <memory>

#include "core/block.hpp"

class IBlockStore
{
public:
    virtual bool store_block(const std::vector<unsigned char> &block_hash, std::shared_ptr<Block> block) = 0;
    virtual std::shared_ptr<Block> get_block(const std::vector<unsigned char> &block_hash) = 0;
    virtual bool remove_block(const std::vector<unsigned char> &block_hash) = 0;
    virtual bool exists(const std::vector<unsigned char> &block_hash) = 0;
};

#endif
