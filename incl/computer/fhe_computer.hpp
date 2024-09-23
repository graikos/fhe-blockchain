#ifndef DIPLO_FHE_COMPUTER_HPP
#define DIPLO_FHE_COMPUTER_HPP

#include <memory>
#include <unordered_set>
#include <atomic>

#include "ast.hpp"
#include "fhe_computation.hpp"
#include "nlohmann/json.hpp"
#include "proofsystem/proofsystem_libsnark.h"

#include "libiop/snark/aurora_snark.hpp"
#include "conv/conv.hpp"

#include "core/interface/computation.hpp"

#include "message.pb.h"

using json = nlohmann::json;

typedef libiop::binary_hash_digest hash_type;

class FHEComputer : public Computation
{
public:
    std::shared_ptr<FHEComputation> computation_;
    std::unique_ptr<ASTree> ast_;
    std::unique_ptr<LibsnarkProofSystem> ps_;

    FHEComputer() = default;
    FHEComputer(const json &comp_json);
    FHEComputer(std::shared_ptr<FHEComputation> computation);

    CryptoContext<DCRTPoly> GetCryptoContext();

    Ciphertext<DCRTPoly> evaluate();
    void generate_constraints(bool eval_output);
    void generate_witness();

    std::vector<unsigned char> output() override;

    void bind_to_data(const std::vector<unsigned char> &data) override;
    std::vector<unsigned char> proof() override;
    void generate_proof() override;
    bool verify_proof(const std::vector<unsigned char> &proof) override;
    uint32_t difficulty() override;

    libiop::aurora_snark_argument<FieldT, hash_type> generate_argument();

    bool verify_argument(const libiop::aurora_snark_argument<FieldT, hash_type> &argument);

    // this one includes output and computes it if not yet computed
    std::vector<unsigned char> serialize(bool include_output) override;

    // this one does not include output
    std::vector<unsigned char> serialize_for_hash();

    std::vector<unsigned char> hash() override;
    std::vector<unsigned char> hash_force() override;

    void set_stop_flag(std::shared_ptr<std::atomic<bool>> stop_flag) override;

    ProtoComputation to_proto() const override;

    static FHEComputer from_proto(const ProtoComputation &proto);

private:
    // NOTE: pass by ref here
    Ciphertext<DCRTPoly> eval(std::shared_ptr<ASTNode> &node, bool eval_mode);
    void init_public_input(std::shared_ptr<ASTNode> node);

    std::vector<unsigned char> proof_;

    std::unordered_set<int> seen_;
    Ciphertext<DCRTPoly> last_res_;

    std::shared_ptr<std::atomic<bool>> stop_flag_;

    bool has_hash_;
    std::vector<unsigned char> hash_;
};

#endif