#include "core/transaction_output.hpp"

#include "util/util.hpp"

#include "sodium.h"

TransactionOutput::TransactionOutput(uint64_t amount, const unsigned char *pubkey) : amount_(amount), public_key_(pubkey, pubkey + crypto_sign_PUBLICKEYBYTES)
{
}

std::vector<unsigned char> TransactionOutput::serialize()
{
    // size is 8 (amount) + public key size (taken from sodium)

    std::vector<unsigned char> res(8 + public_key_.size());

    util::uint64_to_uchar_big_endian(amount_, res.data());

    std::copy(public_key_.begin(), public_key_.end(), res.begin() + 8);

    return res;
}

std::vector<unsigned char> TransactionOutput::hash(bool force)
{
    if (has_hash_ && !force)
    {
        return hash_;
    }
    auto ser = serialize();
    hash_.resize(crypto_generichash_BYTES);
    crypto_generichash(hash_.data(), hash_.size(), ser.data(), ser.size(), nullptr, 0);

    has_hash_ = true;
    return hash_;
}

ProtoTransactionOutput TransactionOutput::to_proto() const
{
    ProtoTransactionOutput po;
    po.set_amount(amount_);
    po.set_public_key(std::string(public_key_.begin(), public_key_.end()));
    return po;
}

TransactionOutput TransactionOutput::from_proto(const ProtoTransactionOutput& proto)
{
    return TransactionOutput(proto.amount(), reinterpret_cast<const unsigned char*>(proto.public_key().c_str()));
}