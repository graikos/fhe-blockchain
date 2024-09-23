#include "computer/fhe_computation.hpp"

#include <sstream>
#include "base64.hpp"
#include "util/util.hpp"

using json = nlohmann::json;

using namespace lbcrypto;

FHEComputation::FHEComputation(const json &computation_json) : is_bound_(false)
{
    // save computation expression
    expression_ = computation_json.at("expression");
    timestamp_ = computation_json.at("timestamp");

    // check if ciphertexts are indeed provided
    if (!computation_json["ciphertexts"].size())
    {
        throw std::invalid_argument("No ciphertexts in computation JSON.");
    }

    // deserialize and save each ciphertext
    for (auto &c : computation_json["ciphertexts"])
    {
        if (!c.is_string())
        {
            throw std::invalid_argument("Ciphertexts must be strings.");
        }

        auto cipher_str = base64::decode(c);
        std::istringstream iss(cipher_str);

        Ciphertext<DCRTPoly> cipher;
        Serial::Deserialize(cipher, iss, SerType::BINARY);

        ciphertexts_.push_back(cipher);
    }

    // if expression contains a multiplication, EvalMultKeys are needed
    if (expression_.find('*') != std::string::npos)
    {
        auto k_str = base64::decode(computation_json.at("eval_mult_key"));
        std::istringstream iss(k_str);
        GetCryptoContext()->DeserializeEvalMultKey(iss, SerType::BINARY);
    }

    auto pubkey_str = base64::decode(computation_json.at("public_key"));
    std::istringstream pubiss(pubkey_str);
    Serial::Deserialize(publicKey_, pubiss, SerType::BINARY);
    // TODO: have other function for this
    // bind_inputs_to_data(seed_data);
}

CryptoContext<DCRTPoly> FHEComputation::GetCryptoContext()
{
    // ciphertexts cannot be empty, so context will be the CC of the first
    return ciphertexts_[0]->GetCryptoContext();
}

void FHEComputation::bind_inputs_to_data(const std::vector<unsigned char> &data)
{
    if (!is_bound_)
    {
        // if no binding has taken place before, archive the original ciphertexts
        // it's possible more binding happen, but original ciphertexts are only saved the first one
        unbound_ciphertexts_archive_.clear();
        for (const auto &c : ciphertexts_)
        {
            unbound_ciphertexts_archive_.push_back(c);
        }
        is_bound_ = true;
    }
    auto cc = GetCryptoContext();

    // buffer to hold a counter and the data to bind the computation with
    // init counter bytes with 0
    std::vector<unsigned char> counter_and_data(sizeof(std::size_t), 0);
    counter_and_data.resize(data.size() + sizeof(std::size_t));

    // copy data to buffer after the counter bytes
    std::copy(data.begin(), data.end(), counter_and_data.begin() + sizeof(std::size_t));

    for (std::size_t i = 0; i < ciphertexts_.size(); ++i)
    {
        std::memcpy(counter_and_data.data(), &i, sizeof(std::size_t));

        auto zero = cc->EncryptZeroDeterministic(publicKey_, counter_and_data);
        ciphertexts_[i] = cc->EvalAdd(zero, ciphertexts_[i]);
    }
}

std::vector<unsigned char> FHEComputation::serialize()
{
    // Output will be prepended, along with its size
    // Serialized computation will be:
    // - Timestamp
    // - Expression size
    // - Expression
    // - Public key size
    // - Public key
    // - EvalMultKey sizes
    // - EvalMultKey
    // - Ciphertext count
    // - For each ciphertext:
    //   - Ciphertext size
    //   - Ciphertext

    std::vector<unsigned char> res(util::uint64_to_vector_big_endian(timestamp_));

    auto expr_size = util::uint64_to_vector_big_endian(expression_.size());
    res.insert(res.end(), expr_size.begin(), expr_size.end());
    res.insert(res.end(), expression_.begin(), expression_.end());

    std::ostringstream pubos;
    Serial::Serialize(publicKey_, pubos, SerType::BINARY);
    auto pubos_str = pubos.str();
    auto pubos_size = util::uint64_to_vector_big_endian(pubos_str.size());
    res.insert(res.end(), pubos_size.begin(), pubos_size.end());
    res.insert(res.end(), pubos_str.begin(), pubos_str.end());

    std::ostringstream evos;
    GetCryptoContext()->SerializeEvalMultKey(evos, SerType::BINARY);
    auto evos_str = evos.str();
    auto evos_size = util::uint64_to_vector_big_endian(evos_str.size());
    res.insert(res.end(), evos_size.begin(), evos_size.end());
    res.insert(res.end(), evos_str.begin(), evos_str.end());

    auto count = util::uint64_to_vector_big_endian(ciphertexts_.size());
    res.insert(res.end(), count.begin(), count.end());

    // since serialization uses the unbound ciphertexts, get from archive if current ones are bound
    auto c_vec = (is_bound_) ? unbound_ciphertexts_archive_ : ciphertexts_;
    for (const auto &c : c_vec)
    {
        std::ostringstream oss;
        Serial::Serialize(c, oss, SerType::BINARY);
        auto oss_str = oss.str();
        auto csize = util::uint64_to_vector_big_endian(oss_str.size());
        res.insert(res.end(), csize.begin(), csize.end());
        res.insert(res.end(), oss_str.begin(), oss_str.end());
    }

    return res;
}

FHEComputation FHEComputation::from_proto(const ProtoComputation &proto)
{
    FHEComputation comp;

    // set bound to false
    comp.is_bound_ = false;

    comp.expression_ = proto.expression();

    std::istringstream pubiss(proto.public_key());
    Serial::Deserialize(comp.publicKey_, pubiss, SerType::BINARY);

    // always assume protobuf carries unbound ciphertext
    // first deserialize ciphertexts to have CryptoContext available
    for (const auto &c : proto.ciphertexts())
    {
        std::istringstream iss(c);

        Ciphertext<DCRTPoly> cipher;
        Serial::Deserialize(cipher, iss, SerType::BINARY);
        comp.ciphertexts_.push_back(cipher);
    }

    if (!proto.evalmult_key().empty())
    {
        std::istringstream iss(proto.evalmult_key());
        comp.GetCryptoContext()->DeserializeEvalMultKey(iss, SerType::BINARY);
    }

    comp.timestamp_ = proto.timestamp();
    return comp;
}