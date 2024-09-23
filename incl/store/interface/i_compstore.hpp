#ifndef DIPLO_COMP_STORE_HPP
#define DIPLO_COMP_STORE_HPP

#include <vector>
#include <memory>

#include "core/interface/computation.hpp"

#include "core/block.hpp"

class ICompStore
{
public:
    virtual bool store_computation(std::shared_ptr<Computation> comp) = 0;
    virtual std::shared_ptr<Computation> get_computation(const std::vector<unsigned char> &comp_hash) = 0;
    virtual bool remove_computation(const std::vector<unsigned char> &comp_hash) = 0;
    virtual bool exists(const std::vector<unsigned char> &comp_hash) = 0;
    virtual std::vector<std::shared_ptr<Computation>> collect_computations(uint32_t target) = 0;
    virtual void spend_block(std::shared_ptr<Block> block) = 0;
    virtual std::vector<std::string> list_comp_hashes() = 0;
};

#endif