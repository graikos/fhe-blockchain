#include "store/mem_blockstore.hpp"

bool MemBlockStore::store_block(const std::vector<unsigned char> &block_hash, std::shared_ptr<Block> block)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = blockhash_to_key(block_hash);
    if (storage_.find(key) != storage_.end())
    {
        // if key exists, do not store again
        return false;
    }
    storage_[key] = block;
    return true;
}

std::shared_ptr<Block> MemBlockStore::get_block(const std::vector<unsigned char> &block_hash)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = blockhash_to_key(block_hash);
    return storage_.at(key);
}

bool MemBlockStore::remove_block(const std::vector<unsigned char> &block_hash)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = blockhash_to_key(block_hash);
    if (storage_.find(key) == storage_.end())
    {
        return false;
    }
    storage_.erase(key);
    return true;
}

bool MemBlockStore::exists(const std::vector<unsigned char> &block_hash)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = blockhash_to_key(block_hash);
    return storage_.find(key) != storage_.end();
}

std::string MemBlockStore::blockhash_to_key(const std::vector<unsigned char> &block_hash)
{
    return std::string(block_hash.begin(), block_hash.end());
}