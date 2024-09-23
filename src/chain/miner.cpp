#include "chain/miner.hpp"
#include "core/merkle.hpp"
#include "util/util.hpp"
#include "core/coinbase_transaction.hpp"

#include <iostream>
#include <ctime>
#include <thread>

#include "base64.hpp"

Miner::Miner(std::shared_ptr<std::atomic<bool>> stop_flag, std::shared_ptr<IMemPool> mem_pool, std::shared_ptr<ICompStore> comp_store)
    : have_result_(false), result(nullptr), stop_flag_(stop_flag), mem_pool_(mem_pool), comp_store_(comp_store)
{
}

void Miner::mine(std::shared_ptr<BlockHeader> prev_header, uint32_t height, uint32_t difficulty, uint64_t reward,
                 const std::vector<std::shared_ptr<Transaction>> &txs, const std::vector<std::shared_ptr<Computation>> &comps,
                 std::shared_ptr<Wallet> wallet)
{

    uint64_t allowed_fee = 0;
    for (const auto &tx: txs)
    {
        // transactions have been selected from the memory pool
        // before adding to the memory pool, their input amounts have been set by the add method
        allowed_fee += tx->fee();
    }
    std::cout << "fee for block I'm mining: " << allowed_fee << std::endl;

    auto cb = std::make_shared<CoinbaseTransaction>(wallet->public_key_.data(), reward + allowed_fee, height);

    std::vector<std::shared_ptr<Transaction>> txs_with_cb = {cb};
    txs_with_cb.insert(txs_with_cb.end(), txs.begin(), txs.end());

    // NOTE: maybe this block is initialized with an old cached hash of the previous block header
    auto new_block = std::make_shared<Block>(prev_header, difficulty, comps, txs_with_cb);

    // Computation is bound to the serialized header + the index of the computation in the vector

    // bind computations and generate proofs
    uint64_t idx = 0;
    auto hser_all = new_block->header_->serialize(false);
    for (const auto &comp : new_block->header_->computations_)
    {
        std::vector<unsigned char> hser(hser_all);
        // serialize header without proofs
        // append computation idx
        auto idxser = util::uint64_to_vector_big_endian(idx++);
        hser.insert(hser.end(), idxser.begin(), idxser.end());

        // bind computation to this data
        comp->set_stop_flag(stop_flag_);
        std::cout << "inside miner, binding comp with idx: " << idx - 1 << "with data hash:" << std::endl;
        unsigned char digest[64];
        crypto_generichash(digest, 64, hser.data(), hser.size(), nullptr, 0);
        std::cout << base64::encode(digest, 64) << std::endl;
        comp->bind_to_data(hser);
        try
        {
            comp->generate_proof();
        }
        catch (std::out_of_range &exc)
        {
            return;
        }
    }

    // force hash the header to be sure that no old cached hash exists at this point
    new_block->header_->hash(true);

    have_result_ = true;
    result = new_block;

    std::cout << base64::encode(cb->serialize().data(), cb->serialize().size()) << std::endl;
}

bool Miner::have_result()
{
    return have_result_;
}