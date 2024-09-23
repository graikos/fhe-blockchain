#ifndef DIPLO_MEM_COMP_STORE_HPP
#define DIPLO_MEM_COMP_STORE_HPP

#include "store/interface/i_compstore.hpp"

#include <vector>
#include <unordered_map>
#include <thread>

class MemCompStore : public ICompStore
{
public:
    MemCompStore() = default;

    bool store_computation(std::shared_ptr<Computation> comp) override;
    std::shared_ptr<Computation> get_computation(const std::vector<unsigned char> &comp_hash) override;
    bool remove_computation(const std::vector<unsigned char> &comp_hash) override;
    bool exists(const std::vector<unsigned char> &comp_hash) override;
    std::vector<std::shared_ptr<Computation>> collect_computations(uint32_t target) override;
    void spend_block(std::shared_ptr<Block> block) override;
    std::vector<std::string> list_comp_hashes() override;

private:
    std::mutex mu_;
    std::unordered_map<std::string, std::shared_ptr<Computation>> storage_;

    std::string comphash_to_key(const std::vector<unsigned char> &comp_hash);
};

#endif