#include "chain/chain.hpp"
#include "util/util.hpp"
#include "core/merkle.hpp"

#include "base64.hpp"

#include <cmath>
#include <iostream>
#include <ctime>

#include <unordered_set>

Chain::Chain(const json &config, std::shared_ptr<IChainstate> chainstate, std::shared_ptr<IBlockStore> block_store, std::shared_ptr<IMemPool> mem_pool, std::shared_ptr<ICompStore> comp_store)
    : config_(config), total_difficulty_(0), chainstate_(chainstate), block_store_(block_store), mem_pool_(mem_pool), comp_store_(comp_store)
{
    auto genesis = create_genesis();
    block_store->store_block(genesis->hash(), genesis);
    chainstate_->add_block(genesis, 0);
    header_chain_.push_back(genesis->header_);
    total_difficulty_ += genesis->header_->difficulty_;
}

Chain::Chain(const json &config, std::shared_ptr<IChainstate> chainstate, std::shared_ptr<IBlockStore> block_store, std::shared_ptr<ICompStore> comp_store, bool is_fork)
    : config_(config), total_difficulty_(0), chainstate_(chainstate), block_store_(block_store), comp_store_(comp_store)
{
}

std::shared_ptr<BlockHeader> Chain::head_header()
{
    std::lock_guard lg(chain_mu_);
    return header_chain_.at(header_chain_.size() - 1);
}

uint32_t Chain::current_height()
{
    std::lock_guard lg(chain_mu_);
    // genesis will be block 0
    return header_chain_.size() - 1;
}

std::shared_ptr<Block> Chain::create_genesis()
{
    auto gen_config = config_.at("chain").at("genesis");

    auto pstr = base64::decode(gen_config.at("public_key"));
    auto pubkey = reinterpret_cast<const unsigned char *>(pstr.c_str());

    uint64_t init_reward = gen_config.at("reward");
    uint64_t init_diff = gen_config.at("difficulty");
    std::time_t timestamp = gen_config.at("timestamp");

    auto exp_hash_str = base64::decode(gen_config.at("hash"));
    std::vector<unsigned char> exp_hash(exp_hash_str.begin(), exp_hash_str.end());

    auto gen_block = std::make_shared<Block>(timestamp, init_diff, pubkey, init_reward);

    if (exp_hash != gen_block->hash())
    {
        std::cout << "Genesis hash not valid." << std::endl;
        std::abort();
    }

    return gen_block;
}

bool Chain::validate_block(std::shared_ptr<Block> block, uint32_t height)
{

    if (block->transactions_.size() == 0)
    {
        std::cout << "No transactions found in block." << std::endl;
        return false;
    }

    std::unordered_set<std::string> temp_utxo_ref;

    uint64_t allowed_fee = 0;

    std::vector<std::vector<unsigned char>> hashes;
    // first, validate transactions
    bool is_cb = true;
    for (const auto &tx : block->transactions_)
    {
        if (is_cb)
        {
            // coinbase validation
            // NOTE: maybe check if TXID of input is 0s
            // if (reward_for_height(height) > tx->outputs_[0]->amount_ + allowed_fee)
            // {
            //     std::cout << "Invalid coinbase reward." << std::endl;
            //     return false;
            // }
            is_cb = false;
            hashes.push_back(tx->TXID());
            continue;
        }

        std::vector<std::vector<unsigned char>> pubkeys;
        for (const auto &inp : tx->inputs_)
        {
            if (temp_utxo_ref.find(util::txid_vout_pair_to_key(inp->TXID_, inp->vout_)) != temp_utxo_ref.end())
            {
                std::cout << "UTXO spent twice in this block." << std::endl;
                return false;
            }

            // insert spent UTXO to temporary k/v store
            temp_utxo_ref.insert(util::txid_vout_pair_to_key(inp->TXID_, inp->vout_));

            // only use as key value store
            try
            {
                pubkeys.push_back(chainstate_->pubkey(inp->TXID_, inp->vout_));
                // retrieve actual amount from chainstate to check for sufficient
                // funds later on
                inp->set_amount(chainstate_->amount(inp->TXID_, inp->vout_));
            }
            catch (const std::out_of_range &e)
            {
                // means UTXO was not found in chainstate
                std::cout << "UTXO referenced was not found in chainstate." << std::endl;
                return false;
            }
        }

        if (!(tx->validate_transaction(pubkeys)))
        {
            std::cout << "Invalid transaction against provided public key." << std::endl;
            return false;
        }

        allowed_fee += tx->fee();

        /**
         * Removed the minimum fee requirement below to simplify checks. Instead, fees will only provide incentive
         * for the miner to pick the transaction, since they are used to sort the transactions in the mempool.
         */

        // // fee should be at minimum equal to the size of the serialzied tx in bytes
        // if (tx->fee() < tx->serialize().size())
        // {
        //     std::cout << "Fee not enough to cover transaction." << std::endl;
        //     return false;
        // }

        hashes.push_back(tx->TXID());
    }

    if (reward_for_height(height) > block->transactions_[0]->outputs_[0]->amount_ + allowed_fee)
    {
        std::cout << "Invalid coinbase reward." << std::endl;
        return false;
    }

    // validate merkle root
    auto actual_merkle_root = Merkle::compute_root(hashes);
    if (actual_merkle_root != block->header_->merkle_root_)
    {
        std::cout << "Merkle root mismatch." << std::endl;
        std::cout << "Actual: " << base64::encode(actual_merkle_root.data(), actual_merkle_root.size()) << std::endl;
        std::cout << "Declared in header: " << base64::encode(block->header_->merkle_root_.data(), block->header_->merkle_root_.size())
                  << std::endl;
        return false;
    }

    return validate_header_unsafe(block->header_, height);
}

// validates headers only, meaning difficulty and proofs
bool Chain::validate_header_unsafe(std::shared_ptr<BlockHeader> header, uint32_t height)
{

    // now check if difficulty is correct
    auto diff_for_height = get_difficulty_for_height(height);
    if (diff_for_height != header->difficulty_)
    {
        std::cout << "Invalid difficutly for block height." << std::endl;
        return false;
    }

    if (header->computations_.size() == 0)
    {
        std::cout << "No computations found in block header." << std::endl;
        return false;
    }

    // now check if computation depth covers difficulty
    uint32_t total_depth = 0;
    for (const auto &comp : header->computations_)
    {
        total_depth += comp->difficulty();
    }
    if (total_depth < diff_for_height)
    {
        std::cout << "Not enought total depth in computations to reach difficulty." << std::endl;
        return false;
    }

    // now bind data to computation and check proof
    // TODO: think about deserialization and if loaded computation is bound or not

    // Binding data will be:
    // - Index of computation in computation vector (8 bytes)
    // - Serialized Header without the proofs (computations included not bound yet)

    uint64_t idx = 0;
    auto hser_all = header->serialize(false);
    for (const auto &comp : header->computations_)
    {
        // serialize header without proofs
        std::vector<unsigned char> hser(hser_all);
        // append computation idx
        auto idxser = util::uint64_to_vector_big_endian(idx++);
        hser.insert(hser.end(), idxser.begin(), idxser.end());

        // bind computation to this data
        comp->bind_to_data(hser);
        if (!(comp->verify_proof(comp->proof())))
        {
            std::cout << "Computation proof not valid." << std::endl;
            return false;
        }
    }
    return true;
}

bool Chain::can_attach(std::shared_ptr<BlockHeader> header)
{
    if (header_chain_.size() == 0)
    {
        // assume all blocks can attach to empty chain
        return true;
    }

    return header->prev_hash() == header_chain_.back()->hash();
}

uint64_t Chain::size()
{
    return header_chain_.size();
}

bool Chain::append_block(std::shared_ptr<Block> block, bool is_already_valid)
{
    std::lock_guard<std::mutex> lg(chain_mu_);
    auto new_height = header_chain_.size();

    std::shared_ptr<BlockHeader> head = header_chain_.back();

    // check if block can be attached
    if (block->header_->prev_hash() != head->hash())
    {
        std::cout << "cannot attach" << std::endl;
        throw std::invalid_argument("Cannot attach block to chain");
    }

    if (!is_already_valid && !validate_block(block, new_height))
    {
        std::cout << "Invalid block." << std::endl;
        return false;
    }

    // before adding, make sure timestamp has been bumped
    // and this is not an attempt to create a chain of similar blocks
    if (block->header_->timestamp_ <= head->timestamp_)
    {
        std::cout << "Timestamp of new block is not greater than current tip." << std::endl;
        return false;
    }

    // block is valid, add to chain
    block->header_->prev_block_header_ = head;
    header_chain_.push_back(block->header_);

    chainstate_->add_block(block, new_height);
    block_store_->store_block(block->hash(), block);
    mem_pool_->spend_block(block);
    comp_store_->spend_block(block);

    total_difficulty_ += block->header_->difficulty_;

    return true;
}

// NOTE: current height locks mutex inside
uint32_t Chain::get_current_epoch()
{
    return current_height() / static_cast<uint32_t>(config_.at("chain").at("blocks_per_epoch"));
}

uint32_t Chain::get_epoch(uint32_t height)
{
    return height / static_cast<uint32_t>(config_.at("chain").at("blocks_per_epoch"));
}

uint64_t Chain::reward_for_height(uint32_t height_to_check)
{
    uint64_t init_reward = config_.at("chain").at("genesis").at("reward");
    // halve init_reward by number of epochs
    return (init_reward >> get_epoch(height_to_check));
}

uint32_t Chain::get_difficulty_for_height(uint32_t height_to_check)
{
    uint32_t difficulty = config_.at("chain").at("genesis").at("difficulty");
    uint32_t epochs = get_epoch(height_to_check);
    std::time_t secs_per_block = config_.at("chain").at("seconds_per_block");
    uint32_t blocks_per_epoch = config_.at("chain").at("blocks_per_epoch");

    for (uint32_t i = 0; i < epochs; ++i)
    {
        auto first = header_chain_[(i - 1) * blocks_per_epoch]->timestamp_;
        auto last = header_chain_[i * blocks_per_epoch - 1]->timestamp_;

        auto actual = last - first;
        auto exp = secs_per_block * blocks_per_epoch;

        // get ratio of how much faster/slower the epoch was
        auto ratio = static_cast<double>(exp) / actual;

        // do not allow very abrupt changes in difficulty
        if (ratio > 4)
        {
            ratio = 4;
        }
        else if (ratio < 0.25)
        {
            ratio = 0.25;
        }

        // increase difficulty (meaning depth) and rounding
        auto res = ratio * difficulty;
        difficulty = std::round(res);
    }
    return difficulty;
}

std::shared_ptr<BlockHeader> Chain::get_header(uint32_t idx)
{
    std::lock_guard<std::mutex> lg(chain_mu_);
    return header_chain_.at(idx);
}

void Chain::print_chain_hashes_force()
{
    std::cout << "+=+=+=+=+=+=+=+=+=+=+=+=" << std::endl;
    for (const auto &h : header_chain_)
    {
        h->hash(true);
    }
    std::cout << "+=+=+=+=+=+=+=+=+=+=+=+=" << std::endl;
}
