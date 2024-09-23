#include "store/mem_compstore.hpp"
#include <iostream>

bool MemCompStore::store_computation(std::shared_ptr<Computation> comp)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = comphash_to_key(comp->hash());
    if (storage_.find(key) != storage_.end())
    {
        // if key exists, do not store again
        return false;
    }
    std::cout << "reach before storing" << std::endl;
    storage_[key] = comp;
    return true;
}

std::shared_ptr<Computation> MemCompStore::get_computation(const std::vector<unsigned char> &comp_hash)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = comphash_to_key(comp_hash);
    return storage_.at(key);
}

bool MemCompStore::remove_computation(const std::vector<unsigned char> &comp_hash)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = comphash_to_key(comp_hash);
    if (storage_.find(key) == storage_.end())
    {
        return false;
    }
    storage_.erase(key);
    return true;
}

bool MemCompStore::exists(const std::vector<unsigned char> &comp_hash)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = comphash_to_key(comp_hash);
    return storage_.find(key) != storage_.end();
}

std::string MemCompStore::comphash_to_key(const std::vector<unsigned char> &comp_hash)
{
    return std::string(comp_hash.begin(), comp_hash.end());
}

std::vector<std::shared_ptr<Computation>> MemCompStore::collect_computations(uint32_t target)
{
    std::cout << "running collect" << std::endl;
    std::lock_guard<std::mutex> lg(mu_);
    std::vector<std::shared_ptr<Computation>> res;
    uint32_t total = 0;
    for (const auto &pair : storage_)
    {
        // naive pick, could optimize for size/difficulty
        res.push_back(pair.second);
        total += pair.second->difficulty();
        if (total >= target)
        {
            // reached target difficulty
            return res;
        }
    }

    // not enough to cover difficulty, can't mine
    return std::vector<std::shared_ptr<Computation>>();
}

std::vector<std::string> MemCompStore::list_comp_hashes()
{
    std::lock_guard<std::mutex> lg(mu_);
    std::vector<std::string> res;
    for (const auto &p : storage_)
    {
        res.push_back(p.first);
    }
    return res;
}

void MemCompStore::spend_block(std::shared_ptr<Block> block)
{
    for (const auto &comp: block->header_->computations_)
    {
        remove_computation(comp->hash());
    }
    return;
}
