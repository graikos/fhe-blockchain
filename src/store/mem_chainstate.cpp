#include "store/mem_chainstate.hpp"
#include <cassert>

#include "util/util.hpp"
#include "core/coinbase_transaction.hpp"

bool MemChainstate::exists(const std::vector<unsigned char> &txid, uint64_t vout)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = pair_to_key(txid, vout);
    return storage_.find(key) != storage_.end();
}

uint32_t MemChainstate::height(const std::vector<unsigned char> &txid, uint64_t vout)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = pair_to_key(txid, vout);
    return storage_.at(key)->height_;
}

bool MemChainstate::coinbase(const std::vector<unsigned char> &txid, uint64_t vout)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = pair_to_key(txid, vout);
    return storage_.at(key)->coinbase_;
}

uint64_t MemChainstate::amount(const std::vector<unsigned char> &txid, uint64_t vout)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = pair_to_key(txid, vout);
    return storage_.at(key)->amount_;
}

std::vector<unsigned char> MemChainstate::pubkey(const std::vector<unsigned char> &txid, uint64_t vout)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = pair_to_key(txid, vout);
    return storage_.at(key)->pubkey_;
}

bool MemChainstate::add_utxo(const std::vector<unsigned char> &txid, uint64_t vout, uint32_t height, bool coinbase, uint64_t amount, const unsigned char *pubkey)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = pair_to_key(txid, vout);
    if (storage_.find(key) != storage_.end())
    {
        // if key already exists, do not add again
        return false;
    }
    // warning: the arguments have shadowed the getters
    auto new_rec = std::make_shared<UTXORecord>(height, coinbase, amount, pubkey);
    storage_[key] = new_rec;
    return true;
}

bool MemChainstate::remove_utxo(const std::vector<unsigned char> &txid, uint64_t vout, bool save_spent)
{
    std::lock_guard<std::mutex> lg(mu_);
    auto key = pair_to_key(txid, vout);
    auto rec = storage_.find(key);
    if (rec == storage_.end())
    {
        // if key does not exists, can't remove
        return false;
    }

    storage_.erase(key);

    if (save_spent)
    {
        // after erasing, add UTXO to spent set
        spent_set_->add_spent_utxo(txid, vout, rec->second);
    }
    return true;
}

void MemChainstate::add_block(std::shared_ptr<Block> block, uint32_t height)
{
    bool is_cb = true;

    // we assume that UTXOs exist, and this block is valid
    // for every transaction in the block, add the outputs as UTXOs
    for (auto const &tx : block->transactions_)
    {

        // add new UTXOs
        uint64_t voutcounter = 0;
        for (auto const &outp : tx->outputs_)
        {
            // NOTE: duplicates?
            add_utxo(tx->TXID(), voutcounter++, height, is_cb, outp->amount_, outp->public_key_.data());
        }

        if (!is_cb)
        {
            for (auto const &inp : tx->inputs_)
            {
                // remove just spent UTXOs
                bool removed = remove_utxo(inp->TXID_, inp->vout_);
                assert(removed);
            }
        }
        else
        {
            is_cb = false;
        }
    }
}

void MemChainstate::rewind_block(std::shared_ptr<Block> block)
{
    bool is_cb = true;
    for (const auto &tx : block->transactions_)
    {
        // remove all UTXOs created by this block
        uint64_t voutcounter = 0;
        for (auto const &outp : tx->outputs_)
        {
            remove_utxo(tx->TXID(), voutcounter++, false);
        }

        // skip input part for coinbase
        if (!is_cb)
        {
            for (const auto &inp : tx->inputs_)
            {
                // retrieve spent UTXO and add again to unspet
                auto undo_rec = spent_set_->get_spent_utxo(inp->TXID_, inp->vout_);
                assert(undo_rec);
                add_utxo(inp->TXID_, inp->vout_, undo_rec->height_, undo_rec->coinbase_, undo_rec->amount_, undo_rec->pubkey_.data());
                // remove undo record
                spent_set_->remove_spent_utxo(inp->TXID_, inp->vout_);
            }
        }
        else
        {
            is_cb = false;
        }
    }
}

std::string MemChainstate::pair_to_key(const std::vector<unsigned char> &txid, uint64_t vout)
{
    std::string res(txid.begin(), txid.end());
    auto voutser = util::uint64_to_vector_big_endian(vout);
    res.append(voutser.begin(), voutser.end());
    return res;
}

std::vector<std::pair<std::vector<unsigned char>, uint64_t>> MemChainstate::filter_by_pubkey(const unsigned char *pubkey)
{
    std::lock_guard<std::mutex> lg(mu_);

    std::vector<unsigned char> pubkey_vec(pubkey, pubkey + crypto_sign_PUBLICKEYBYTES);

    std::vector<std::pair<std::vector<unsigned char>, uint64_t>> res;

    for (const auto &p : storage_)
    {
        if (p.second->pubkey_ == pubkey_vec)
        {
            // deserialize txid from string, last 8 bytes are vout
            std::vector<unsigned char> txid(p.first.begin(), p.first.end() - 8);
            uint64_t vout = util::chars_to_uint64_big_endian(reinterpret_cast<const unsigned char *>(p.first.c_str() + txid.size()));
            auto pair = std::make_pair<>(txid, vout);
            res.push_back(pair);
        }
    }
    return res;
}

bool SpentSet::add_spent_utxo(const std::vector<unsigned char> &txid, uint64_t vout, std::shared_ptr<UTXORecord> record)
{
    auto key = util::txid_vout_pair_to_key(txid, vout);
    if (spent_storage_.find(key) != spent_storage_.end())
    {
        // if key already exists, do not add again
        return false;
    }
    spent_storage_[key] = record;
    return true;
}

std::shared_ptr<UTXORecord> SpentSet::get_spent_utxo(const std::vector<unsigned char> &txid, uint64_t vout)
{
    auto key = util::txid_vout_pair_to_key(txid, vout);
    if (spent_storage_.find(key) == spent_storage_.end())
    {
        // key does not exist
        std::shared_ptr<UTXORecord> rec(nullptr);
        return rec;
    }
    return spent_storage_[key];
}

bool SpentSet::remove_spent_utxo(const std::vector<unsigned char> &txid, uint64_t vout)
{
    auto key = util::txid_vout_pair_to_key(txid, vout);
    if (spent_storage_.find(key) == spent_storage_.end())
    {
        // key does not exist
        return false;
    }
    spent_storage_.erase(key);
    return true;
}