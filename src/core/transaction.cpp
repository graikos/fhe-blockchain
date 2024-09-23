#include "core/transaction.hpp"

#include "util/util.hpp"

Transaction::Transaction(const std::vector<std::shared_ptr<TransactionInput>> &inputs, const std::vector<std::shared_ptr<TransactionOutput>> &outputs)
    : inputs_(inputs), outputs_(outputs), has_txid_(false)
{
    try
    {
        TXID();
    }
    catch (const std::exception &e)
    {
    }
}

Transaction::Transaction(const std::vector<std::shared_ptr<TransactionInput>> &inputs, const unsigned char *self_pubkey, const unsigned char *rec_pubkey, uint64_t amount, uint64_t fee)
    : inputs_(inputs), has_txid_(false)
{
    // won't do TXID computation in this constructor, since the inputs cannot have been signed, they don't even have outputs

    // here it is assumed that inputs have amounts provided
    // check if inputs suffice

    uint64_t total = 0;

    for (const auto &inp : inputs_)
    {
        total += inp->amount();
    }

    if (total < amount + fee)
    {
        throw std::logic_error("Insufficient inputs provided.");
    }

    // create main output
    auto main_output = std::make_shared<TransactionOutput>(amount, rec_pubkey);
    add_output(main_output);

    if (total > amount + fee)
    {
        // create output to self for the change
        auto change_output = std::make_shared<TransactionOutput>(total - amount - fee, self_pubkey);
        add_output(change_output);
    }
}

Transaction::Transaction(const Transaction &other)
{
    for (auto const &inp : other.inputs_)
    {
        // copy inputs
        TransactionInput copy_inp(*inp);
        inputs_.push_back(std::make_shared<TransactionInput>(copy_inp));
    }

    for (auto const &outp : other.outputs_)
    {
        // copy outputs
        TransactionOutput copy_outp(*outp);
        outputs_.push_back(std::make_shared<TransactionOutput>(copy_outp));
    }

    has_txid_ = other.has_txid_;
    TXID_ = other.TXID_;
}

bool Transaction::operator==(Transaction &other)
{
    return TXID() == other.TXID();
}

void Transaction::add_input(std::shared_ptr<TransactionInput> new_input)
{
    inputs_.push_back(new_input);
}

void Transaction::add_output(std::shared_ptr<TransactionOutput> new_output)
{
    outputs_.push_back(new_output);
}

/**
 * Structure of the serialized transaction will be:
 * - Input count (8 bytes)
 * - Inputs
 * - Output count (8 bytes)
 * - Outputs
 */
std::vector<unsigned char> Transaction::serialize()
{
    // init with input count
    std::vector<unsigned char> res = util::uint64_to_vector_big_endian(inputs_.size());
    // append serialized inputs
    for (const auto &inp : inputs_)
    {
        auto ser = inp->serialize();
        res.insert(res.end(), ser.begin(), ser.end());
    }
    // append output count
    auto outcountvec = util::uint64_to_vector_big_endian(outputs_.size());
    res.insert(res.end(), outcountvec.begin(), outcountvec.end());

    // append serialized outputs
    for (const auto &outp : outputs_)
    {
        auto ser = outp->serialize();
        res.insert(res.end(), ser.begin(), ser.end());
    }
    return res;
}

std::vector<unsigned char> Transaction::TXID()
{
    for (const auto &inp : inputs_)
    {
        // TODO: maybe coinbase should not have this restriction
        if (!inp->is_signed())
        {
            std::cout << "is not signed" << std::endl;
            throw std::invalid_argument("Cannot compute TXID with unsigned inputs.");
        }
    }

    if (has_txid_)
    {
        return TXID_;
    }

    compute_txid();
    return TXID_;
}

void Transaction::compute_txid()
{
    auto serial_data = serialize();

    TXID_.resize(crypto_generichash_BYTES);
    crypto_generichash(TXID_.data(), crypto_generichash_BYTES, serial_data.data(), serial_data.size(), nullptr, 0);

    has_txid_ = true;
}

void Transaction::sign(const unsigned char *self_pubkey, const unsigned char *privkey)
{
    for (const auto &inp : inputs_)
    {
        if (inp->is_signed())
        {
            throw std::invalid_argument("Cannot sign already signed inputs.");
        }
    }

    // self_pubkey will be the equiv of scriptpubkey of referred output from the inputs
    std::vector<std::vector<unsigned char>> sigs;
    for (auto &inp : inputs_)
    {
        // set current sig field equal to pubkey of self
        inp->set_temp_sig_size(crypto_sign_PUBLICKEYBYTES);
        inp->set_signature(self_pubkey, true);

        auto serial_tx = serialize();

        std::vector<unsigned char> new_sig(crypto_sign_BYTES);
        crypto_sign_detached(new_sig.data(), nullptr, serial_tx.data(), serial_tx.size(), privkey);

        sigs.push_back(new_sig);

        // revert size and clear current signature for the other inputs to be signed
        inp->revert_sig_size();
        inp->clear_signature();
    }

    for (uint64_t i = 0; i < inputs_.size(); ++i)
    {
        inputs_[i]->set_signature(sigs[i].data());
    }
}

uint64_t Transaction::fee()
{
    uint64_t in_total = 0;
    for (const auto &inp : inputs_)
    {
        in_total += inp->amount();
    }

    uint64_t out_total = 0;
    for (const auto &outp : outputs_)
    {
        out_total += outp->amount_;
    }

    if (in_total < out_total)
    {
        std::cout << "in: " << in_total << " out: " << out_total << std::endl;
        throw std::invalid_argument("Insufficient inputs provided.");
    }

    return in_total - out_total;
}

bool Transaction::validate_transaction(const std::vector<std::vector<unsigned char>> &pubkeys) const
{

    // check that the funds are sufficient
    uint64_t in_total = 0;
    for (const auto &inp : inputs_)
    {
        if (!inp->is_signed())
        {
            std::cout << "input not signed" << std::endl;
            return false;
        }
        in_total += inp->amount();
    }

    uint64_t out_total = 0;
    for (const auto &outp : outputs_)
    {
        out_total += outp->amount_;
    }

    if (in_total < out_total)
    {
        std::cout << "insufficient funds" << std::endl;
        return false;
    }

    // to check signatures, first copy this transaction
    Transaction copy_tx(*this);

    // first clear all input sigs
    for (auto &inp : copy_tx.inputs_)
    {
        // ensure correct size
        inp->revert_sig_size();
        inp->clear_signature();
    }

    uint64_t idx = 0;
    for (auto &inp : copy_tx.inputs_)
    {
        // set current sig field equal to pubkey of referrenced output
        inp->set_temp_sig_size(crypto_sign_PUBLICKEYBYTES);
        inp->set_signature(pubkeys[idx].data(), true);

        auto serial_tx = copy_tx.serialize();

        // verify against the signature of the input in the original TX
        if (crypto_sign_verify_detached(inputs_[idx]->sig_.data(), serial_tx.data(), serial_tx.size(), pubkeys[idx].data()) != 0)
        {
            std::cout << "signature invalid" << std::endl;
            return false;
        }

        // revert size and clear current signature for the other inputs to be signed
        inp->revert_sig_size();
        inp->clear_signature();
        ++idx;
    }
    return true;
}

// void Transaction::sign_transaction(const std::vector<unsigned char> &public_key, const unsigned char *secret_key)
// {
//     Transaction copy_tx(*this);

//     // first clear all input sigs
//     for (auto &inp : copy_tx.inputs_)
//     {
//         // ensure correct size
//         inp->revert_sig_size();
//         inp->clear_signature();
//     }

//     std::vector<std::vector<unsigned char>> sigs;

//     for (auto &inp : copy_tx.inputs_)
//     {
//         // set current sig field equal to pubkey of referrenced output
//         inp->set_temp_sig_size(crypto_sign_PUBLICKEYBYTES);
//         // since this is a method to sign, all outputs will refer to the secret key's public key
//         inp->set_signature(public_key.data(), true);

//         auto serial_tx = copy_tx.serialize();

//         std::vector<unsigned char> sig(crypto_sign_BYTES);
//         crypto_sign(sig.data(), nullptr, serial_tx.data(), serial_tx.size(), secret_key);
//         sigs.push_back(sig);

//         // revert size and clear current signature for the other inputs to be signed
//         inp->revert_sig_size();
//         inp->clear_signature();
//     }

//     for (int i = 0; i < sigs.size(); ++i)
//     {
//         inputs_[i]->set_signature(sigs[i].data());
//     }
// }

ProtoTransaction Transaction::to_proto() const
{
    ProtoTransaction pt;
    for (const auto &inp : inputs_)
    {
        pt.add_inputs()->CopyFrom(inp->to_proto());
    }

    for (const auto &outp : outputs_)
    {
        pt.add_outputs()->CopyFrom(outp->to_proto());
    }
    return pt;
}

Transaction Transaction::from_proto(const ProtoTransaction &proto, bool is_coinbase)
{
    std::vector<std::shared_ptr<TransactionInput>> inputs;
    std::vector<std::shared_ptr<TransactionOutput>> outputs;

    for (const auto &proto_input : proto.inputs())
    {
        inputs.push_back(std::make_shared<TransactionInput>(TransactionInput::from_proto(proto_input, is_coinbase)));
    }

    for (const auto &proto_output : proto.outputs())
    {
        outputs.push_back(std::make_shared<TransactionOutput>(TransactionOutput::from_proto(proto_output)));
    }

    return Transaction(inputs, outputs);
}