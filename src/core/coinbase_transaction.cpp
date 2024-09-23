#include "core/coinbase_transaction.hpp"
#include "util/util.hpp"

// CoinbaseTransaction::CoinbaseTransaction(std::vector<std::shared_ptr<Computation>> &comps, const unsigned char *pubkey, uint64_t amount)
//     : computations_(comps), public_key_(pubkey, pubkey + crypto_sign_PUBLICKEYBYTES), amount_(amount)
// {
//     // create the inputs, one for each computation
//     for (const auto &comp : computations_)
//     {
//         // create input
//         // change sig bytes to the size of the serialized computation
//         // add serialized computation to signature field
//         // no need for amount

//         // coinbase input TXID should be all zeros, and vout max val
//         std::vector<unsigned char> txid(crypto_generichash_BYTES, 0);
//         uint64_t vout = -1;

//         auto ser_comp = comp->serialize();

//         auto new_input = std::make_shared<TransactionInput>(txid, vout);
//         new_input->set_temp_sig_size(ser_comp.size());
//         // explicitly tell set_signature to treat this input like it's signed, so it can pass the checks
//         new_input->set_signature(ser_comp.data(), false);

//         // add input to this transaction
//         add_input(new_input);
//     }

//     // now create the coinbase output
//     auto new_output = std::make_shared<TransactionOutput>(amount_, pubkey);
//     add_output(new_output);
// }

CoinbaseTransaction::CoinbaseTransaction(const unsigned char *pubkey, uint64_t amount, uint32_t height)
{
    // create the empty input
    // set sig_size to be 0
    // manually set is_signed_ to true so the input can pass any checks
    // no need to add signature

    // coinbase input TXID should be all zeros, and vout max val
    std::vector<unsigned char> txid(crypto_generichash_BYTES, 0);
    uint64_t vout = -1;

    auto new_input = std::make_shared<TransactionInput>(txid, vout);

    // include height in signature field to have unique coinbase TXID even if block is empty at different heights
    unsigned char height_bytes[4];
    util::uint32_to_uchar_big_endian(height, height_bytes);

    new_input->set_temp_sig_size(4);
    new_input->set_signature(height_bytes);

    add_input(new_input);

    auto new_output = std::make_shared<TransactionOutput>(amount, pubkey);
    add_output(new_output);

    has_txid_ = false;
}

uint64_t CoinbaseTransaction::amount()
{
    return outputs_[0]->amount_;
}