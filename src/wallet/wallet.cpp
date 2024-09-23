#include "wallet/wallet.hpp"

#include <iostream>

Wallet::Wallet(const json &config)
{
    auto dec_public_key = base64::decode(config.at("ed25519").at("public_key"));
    // public_key_.assign(dec_public_key.begin(), dec_public_key.end());
    public_key_ = std::vector<unsigned char>(dec_public_key.begin(), dec_public_key.end());

    auto dec_secret_key = base64::decode(config.at("ed25519").at("secret_key"));
    // secret_key_.assign(dec_secret_key.begin(), dec_secret_key.end());
    secret_key_ = std::vector<unsigned char>(dec_secret_key.begin(), dec_secret_key.end());

    std::cout << "++++++++++++++++++++++++++++" << std::endl;
    std::cout << "Loaded wallet keypair: " << std::endl;
    std::cout << "Public key: [" << base64::encode(public_key_.data(), public_key_.size()) << "]" << std::endl;
    std::cout << "Secret key: [" << base64::encode(secret_key_.data(), secret_key_.size()) << "]" << std::endl;
    std::cout << "++++++++++++++++++++++++++++" << std::endl;
}

Wallet::Wallet()
{
    gen_keys();
}

std::shared_ptr<Transaction> Wallet::new_transaction(const std::vector<unsigned char> &rec_pub_key, uint64_t amount, uint64_t fee)
{
    std::lock_guard<std::mutex> lg(wallet_mu_);

    uint64_t total = amount + fee;
    uint64_t current_funds = 0;
    bool covered = false;

    std::vector<std::shared_ptr<TransactionInput>> inputs;
    for (const auto &pair : coins_)
    {
        auto coin = pair.second;
        inputs.push_back(coin);
        current_funds += coin->amount();
        std::cout << "Collected coin, amount: " << coin->amount() << std::endl;
        if (current_funds >= total)
        {
            covered = true;
            break;
        }
    }

    if (!covered)
    {
        throw std::runtime_error("Not enough coins to create transaction.");
    }

    auto tx = std::make_shared<Transaction>(inputs, public_key_.data(), rec_pub_key.data(), amount, fee);

    // now wallet needs to sign the inputs
    tx->sign(public_key_.data(), secret_key_.data());

    /**
     * NOTE: Do not remove the coins used yet. They will be removed once the transaction is added
     * to the blockchain. Transactions in the MemPool are allowed to reference the same UTXO twice.
     * However, after being spent, all transactions doing this should be removed together.
     */

    return tx;
}

void Wallet::gen_keys()
{
    public_key_.resize(crypto_sign_PUBLICKEYBYTES);
    secret_key_.resize(crypto_sign_SECRETKEYBYTES);
    crypto_sign_keypair(public_key_.data(), secret_key_.data());

    std::cout << "++++++++++++++++++++++++++++" << std::endl;
    std::cout << "Generated wallet keypair: " << std::endl;
    std::cout << "Public key: " << base64::encode(public_key_.data(), public_key_.size()) << std::endl;
    std::cout << "Secret key: " << base64::encode(secret_key_.data(), secret_key_.size()) << std::endl;
    std::cout << "++++++++++++++++++++++++++++" << std::endl;
}

void Wallet::filter_transaction(std::shared_ptr<Transaction> tx)
{
    std::lock_guard<std::mutex> lg(wallet_mu_);
    unsafe_filter_transaction(tx);
}

void Wallet::spend_transaction(std::shared_ptr<Transaction> tx)
{
    std::lock_guard<std::mutex> lg(wallet_mu_);
    unsafe_spend_transaction(tx);
}

void Wallet::filter_block(std::shared_ptr<Block> block)
{
    std::lock_guard<std::mutex> lg(wallet_mu_);
    for (const auto &tx : block->transactions_)
    {
        unsafe_filter_transaction(tx);
    }
}

void Wallet::spend_block(std::shared_ptr<Block> block)
{
    std::lock_guard<std::mutex> lg(wallet_mu_);
    for (const auto &tx : block->transactions_)
    {
        unsafe_spend_transaction(tx);
    }
}

void Wallet::unsafe_filter_transaction(std::shared_ptr<Transaction> tx)
{
    uint64_t idx = 0;
    for (const auto &outp : tx->outputs_)
    {
        if (outp->public_key_ == public_key_)
        {
            // this UTXO belongs to this wallet
            auto new_inp = std::make_shared<TransactionInput>(tx->TXID(), idx);
            new_inp->set_amount(outp->amount_);
            coins_[util::txid_vout_pair_to_key(tx->TXID(), idx)] = new_inp;
            std::cout << "+++++++++++++++++++" << std::endl;
            std::cout << "New wallet coin:" << std::endl;
            std::cout << "TXID: " << base64::encode(tx->TXID().data(), tx->TXID().size()) << std::endl;
            std::cout << "vout: " << idx << std::endl;
            std::cout << "amount: " << outp->amount_ << std::endl;
            std::cout << "+++++++++++++++++++" << std::endl;
        }
        // TODO: remove after debug
        else
        {
            std::cout << "did not add to wallet" << std::endl;
            std::cout << "my pubkey: " << base64::encode(public_key_.data(), public_key_.size()) << std::endl;
            std::cout << "output key: " << base64::encode(outp->public_key_.data(), outp->public_key_.size()) << std::endl;
        }
        ++idx;
    }
}

bool Wallet::unsafe_remove_coin(const std::vector<unsigned char> &txid, uint64_t vout)
{
    return coins_.erase(util::txid_vout_pair_to_key(txid, vout)) != 0;
}

void Wallet::unsafe_spend_transaction(std::shared_ptr<Transaction> tx)
{
    for (const auto &inp : tx->inputs_)
    {
        // remove coin from this wallet
        if (unsafe_remove_coin(inp->TXID_, inp->vout_))
        {
            std::cout << "------------------------" << std::endl;
            std::cout << "Wallet coin spent:" << std::endl;
            std::cout << "TXID: " << base64::encode(inp->TXID_.data(), inp->TXID_.size()) << std::endl;
            std::cout << "vout: " << inp->vout_ << std::endl;
            std::cout << "------------------------" << std::endl;
        }
    }
}

void Wallet::rescan(const std::vector<std::shared_ptr<BlockHeader>> &chain, std::shared_ptr<IBlockStore> block_store)
{
    {
        std::lock_guard<std::mutex> lg(wallet_mu_);
        coins_.clear();
    }

    for (const auto &header : chain)
    {
        auto block = block_store->get_block(header->hash());
        // remove any UTXO that belong to this wallet and were used
        spend_block(block);
        // add any new UTXOs that belong to this wallet
        filter_block(block);
    }
}