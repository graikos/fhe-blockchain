#include "sodium.h"

#include "util/util.hpp"
#include "computer/fhe_computer.hpp"

#include "core/block_header.hpp"

using json = nlohmann::json;

FHEComputer::FHEComputer(const json &comp_json) : computation_(std::make_shared<FHEComputation>(comp_json)),
                                                  ast_(std::make_unique<ASTree>(computation_->expression_)),
                                                  last_res_(nullptr), has_hash_(false)
{
}

FHEComputer::FHEComputer(std::shared_ptr<FHEComputation> computation) : computation_(computation), ast_(std::make_unique<ASTree>(computation_->expression_)), last_res_(nullptr), has_hash_(false)
{
}

Ciphertext<DCRTPoly> FHEComputer::evaluate()
{

    // simple evaluation will use CryptoContext

    last_res_ = eval(ast_->root_, true);
    return last_res_;
}

void FHEComputer::generate_constraints(bool eval_output)
{
    // NOTE: for some reason it's satisfied only with (0+1)*(0-1)
    ps_ = std::make_unique<LibsnarkProofSystem>(computation_->GetCryptoContext());
    Ciphertext<DCRTPoly> out_ctxt;
    if (eval_output)
    {
        // this case will be used for proof generation
        // result of this is also saved to last_res_, so it will be included in the transmitted block
        out_ctxt = evaluate();
    }
    else
    {
        // this will be used for proof verification
        out_ctxt = last_res_;
    }

    ps_->SetMode(PROOFSYSTEM_MODE::PROOFSYSTEM_MODE_CONSTRAINT_GENERATION);
    seen_.clear();
    init_public_input(ast_->root_);
    auto vars_out = *(ps_->ConstrainPublicOutput(out_ctxt));
    out_ctxt = eval(ast_->root_, false);
    ps_->FinalizeOutputConstraints(out_ctxt, vars_out);

    const r1cs_constraint_system<FieldT> constraint_system = ps_->pb.get_constraint_system();

    cout << "#inputs:      " << constraint_system.num_inputs() << endl;
    cout << "#variables:   " << constraint_system.num_variables() << endl;
    cout << "#constraints: " << constraint_system.num_constraints() << endl;
    cout << "#aux: " << ps_->pb.auxiliary_input().size() << endl;

    bool satisfied = constraint_system.is_satisfied(ps_->pb.primary_input(), ps_->pb.auxiliary_input());
    cout << "satisfied:    " << std::boolalpha << satisfied << endl;
}

void FHEComputer::generate_witness()
{
    ps_->SetMode(PROOFSYSTEM_MODE::PROOFSYSTEM_MODE_WITNESS_GENERATION);
    seen_.clear();
    init_public_input(ast_->root_);
    // NOTE: maybe incorrect
    auto out_ctxt = eval(ast_->root_, false);

    const auto pb = ps_->pb;

    const r1cs_constraint_system<FieldT> constraint_system = ps_->pb.get_constraint_system();

    cout << "#inputs:      " << constraint_system.num_inputs() << endl;
    cout << "#variables:   " << constraint_system.num_variables() << endl;
    cout << "#constraints: " << constraint_system.num_constraints() << endl;
    cout << "#aux: " << pb.auxiliary_input().size() << endl;

    bool satisfied = constraint_system.is_satisfied(pb.primary_input(), pb.auxiliary_input());
    cout << "satisfied:    " << std::boolalpha << satisfied << endl;
}

libiop::aurora_snark_argument<FieldT, hash_type> FHEComputer::generate_argument()
{
    const size_t security_parameter = 128;
    const size_t RS_extra_dimensions = 2;
    const size_t FRI_localization_parameter = 3;
    const libiop::LDT_reducer_soundness_type ldt_reducer_soundness_type = libiop::LDT_reducer_soundness_type::optimistic_heuristic;
    const libiop::FRI_soundness_type fri_soundness_type = libiop::FRI_soundness_type::heuristic;
    const libiop::field_subset_type domain_type = libiop::affine_subspace_type;
    const bool make_zk = false;

    libiop::r1cs_constraint_system<FieldT> liop_cs;
    libsnark_to_libiop_r1cs_constraint_system(ps_->pb.get_constraint_system(), liop_cs);
    std::cout << "Just converted to libiop constraint system:" << std::endl;
    cout << "#inputs:      " << liop_cs.num_inputs() << endl;
    cout << "#variables:   " << liop_cs.num_variables() << endl;
    cout << "#constraints: " << liop_cs.num_constraints() << endl;

    pad_constraint_system_for_aurora(liop_cs);
    std::cout << "Padding libiop constraint system:" << std::endl;
    cout << "#inputs:      " << liop_cs.num_inputs() << endl;
    cout << "#variables:   " << liop_cs.num_variables() << endl;
    cout << "#constraints: " << liop_cs.num_constraints() << endl;

    libiop::r1cs_primary_input<FieldT> cs_primary_input = ps_->pb.primary_input();
    libiop::r1cs_auxiliary_input<FieldT> cs_auxiliary_input = ps_->pb.auxiliary_input();

    pad_primary_input_to_match_cs(liop_cs, cs_primary_input);
    pad_auxiliary_input_to_match_cs(liop_cs, cs_auxiliary_input);

    libiop::aurora_snark_parameters<FieldT, hash_type> params(
        security_parameter,
        ldt_reducer_soundness_type,
        fri_soundness_type,
        libiop::blake2b_type,
        FRI_localization_parameter,
        RS_extra_dimensions,
        make_zk,
        domain_type,
        liop_cs.num_constraints(),
        liop_cs.num_variables());

    const libiop::aurora_snark_argument<FieldT, hash_type> argument = aurora_snark_prover<FieldT>(
        liop_cs,
        cs_primary_input,
        cs_auxiliary_input,
        params);

    printf("iop size in bytes %lu\n", argument.IOP_size_in_bytes());
    printf("bcs size in bytes %lu\n", argument.BCS_size_in_bytes());
    printf("argument size in bytes %lu\n", argument.size_in_bytes());

    return argument;
}

bool FHEComputer::verify_argument(const libiop::aurora_snark_argument<FieldT, hash_type> &argument)
{
    const size_t security_parameter = 128;
    const size_t RS_extra_dimensions = 2;
    const size_t FRI_localization_parameter = 3;
    const libiop::LDT_reducer_soundness_type ldt_reducer_soundness_type = libiop::LDT_reducer_soundness_type::optimistic_heuristic;
    const libiop::FRI_soundness_type fri_soundness_type = libiop::FRI_soundness_type::heuristic;
    const libiop::field_subset_type domain_type = libiop::affine_subspace_type;
    const bool make_zk = false;

    libiop::r1cs_constraint_system<FieldT> liop_cs;
    libsnark_to_libiop_r1cs_constraint_system(ps_->pb.get_constraint_system(), liop_cs);
    std::cout << "Just converted to libiop constraint system:" << std::endl;
    cout << "#inputs:      " << liop_cs.num_inputs() << endl;
    cout << "#variables:   " << liop_cs.num_variables() << endl;
    cout << "#constraints: " << liop_cs.num_constraints() << endl;

    pad_constraint_system_for_aurora(liop_cs);
    std::cout << "Padding libiop constraint system:" << std::endl;
    cout << "#inputs:      " << liop_cs.num_inputs() << endl;
    cout << "#variables:   " << liop_cs.num_variables() << endl;
    cout << "#constraints: " << liop_cs.num_constraints() << endl;

    libiop::r1cs_primary_input<FieldT> cs_primary_input = ps_->pb.primary_input();
    libiop::r1cs_auxiliary_input<FieldT> cs_auxiliary_input = ps_->pb.auxiliary_input();

    pad_primary_input_to_match_cs(liop_cs, cs_primary_input);
    pad_auxiliary_input_to_match_cs(liop_cs, cs_auxiliary_input);

    libiop::aurora_snark_parameters<FieldT, hash_type> params(
        security_parameter,
        ldt_reducer_soundness_type,
        fri_soundness_type,
        libiop::blake2b_type,
        FRI_localization_parameter,
        RS_extra_dimensions,
        make_zk,
        domain_type,
        liop_cs.num_constraints(),
        liop_cs.num_variables());

    return aurora_snark_verifier<FieldT, hash_type>(
        liop_cs,
        cs_primary_input,
        argument,
        params);
}

// void FHEComputer::generate_constraints()
// {
//     std::cout << "here" << std::endl;
//     auto c1 = computation_->ciphertexts_[1];
//     auto c2 = computation_->ciphertexts_[2];
//     auto c_mult = GetCryptoContext()->EvalMultNoRelin(c1, c2);
//     auto c_relin = GetCryptoContext()->Relinearize(c_mult);

//     // TODO: also test with the pointer
//     LibsnarkProofSystem ps(GetCryptoContext());
//     ps.SetMode(PROOFSYSTEM_MODE::PROOFSYSTEM_MODE_CONSTRAINT_GENERATION);

//     ps.PublicInput(c1);
//     ps.PublicInput(c2);
//     // TODO important: only add public input before adding any other constraints!
//     auto vars_out = *ps.ConstrainPublicOutput(c_relin);

//     c_mult = ps.EvalMultNoRelin(c1, c2);
//     c_relin = ps.Relinearize(c_mult);

//     ps.FinalizeOutputConstraints(c_relin, vars_out);
// 	const auto pb                                          = ps.pb;

//     const r1cs_constraint_system<FieldT> constraint_system = ps.pb.get_constraint_system();

//     cout << "#inputs:      " << constraint_system.num_inputs() << endl;
//     cout << "#variables:   " << constraint_system.num_variables() << endl;
//     cout << "#constraints: " << constraint_system.num_constraints() << endl;
//     cout << "#aux: " << pb.auxiliary_input().size() << endl;

//     bool satisfied = constraint_system.is_satisfied(pb.primary_input(), pb.auxiliary_input());
//     cout << "satisfied:    " << std::boolalpha << satisfied << endl;
// }

Ciphertext<DCRTPoly> FHEComputer::eval(std::shared_ptr<ASTNode> &node, bool eval_mode)
{
    if (stop_flag_ && *stop_flag_)
    {
        throw std::out_of_range("stop flag");
    }

    // leaves correspond to ciphertexts as given in the original computation
    if (node->is_leaf)
    {
        // return computation_->ciphertexts_.at(node->val_)->Clone();
        return computation_->ciphertexts_.at(node->val_);
    }

    // if not leaf, then node should have both children
    if (!node->left_child_ || !node->right_child_)
    {
        throw std::runtime_error("Internal ASTNode should have both chldren.");
    }

    // collect results from lower level
    auto c_left = eval(node->left_child_, eval_mode);
    auto c_right = eval(node->right_child_, eval_mode);

    auto left_depth = node->left_child_->depth_;
    auto right_depth = node->right_child_->depth_;

    // bring both children ciphertexts to the same level
    auto depth_diff = left_depth - right_depth;
    if (depth_diff != 0)
    {
        auto &target = (depth_diff > 0) ? c_right : c_left;
        depth_diff = std::abs(depth_diff);
        while (depth_diff--)
        {
            if (eval_mode)
            {
                target = GetCryptoContext()->Relinearize(target);
                target = GetCryptoContext()->Rescale(target);
            }
            else
            {
                target = ps_->Relinearize(target);
                target = ps_->Rescale(target);
            }
        }
    }

    if (node->op_ == "+")
    {
        std::cout << "+" << std::endl;
        if (eval_mode)
        {
            return GetCryptoContext()->EvalAdd(c_left, c_right);
        }
        return ps_->EvalAdd(c_left, c_right);
    }
    else if (node->op_ == "-")
    {
        std::cout << "-" << std::endl;
        if (eval_mode)
        {
            return GetCryptoContext()->EvalSub(c_left, c_right);
        }
        return ps_->EvalSub(c_left, c_right);
    }
    else if (node->op_ == "*")
    {
        std::cout << "*" << std::endl;
        if (eval_mode)
        {
            auto c_res = GetCryptoContext()->EvalMultNoRelin(c_left, c_right);
            return c_res;
        }
        auto c_mult = ps_->EvalMultNoRelin(c_left, c_right);
        return c_mult;
    }
    else
    {
        throw std::invalid_argument("Invalid op in ASTNode.");
    }
}

void FHEComputer::init_public_input(std::shared_ptr<ASTNode> node)
{
    // leaves correspond to ciphertexts as given in the original computation
    if (node->is_leaf)
    {
        if (seen_.find(node->val_) == seen_.end())
        {
            // if ciphertext input has not been seen before, declare as PublicInput and set as seen
            ps_->PublicInput(computation_->ciphertexts_.at(node->val_));
            seen_.insert(node->val_);
        }
        return;
    }

    // if not leaf, then node should have both children
    if (!node->left_child_ || !node->right_child_)
    {
        throw std::runtime_error("Internal ASTNode should have both chldren.");
    }

    // collect results from lower level
    init_public_input(node->left_child_);
    init_public_input(node->right_child_);
}

CryptoContext<DCRTPoly> FHEComputer::GetCryptoContext()
{
    return computation_->GetCryptoContext();
}

void FHEComputer::bind_to_data(const std::vector<unsigned char> &data)
{
    computation_->bind_inputs_to_data(data);
}

std::vector<unsigned char> FHEComputer::proof()
{
    if (proof_.size() == 0)
    {
        throw std::out_of_range("Proof not found for this computation.");
    }

    return proof_;
}

void FHEComputer::generate_proof()
{
    // note: here, apart from the constraints, a witness should be called, but because of zkOpenFHE having
    // an issue with this, we're using just constraint generation, which inadvertently generates a witness
    generate_constraints(true);

    auto arg = generate_argument();
    std::ostringstream oss;
    arg.serialize(oss);
    auto st = oss.str();

    proof_.assign(st.begin(), st.end());
    // proof_ = std::vector<unsigned char>(st.begin(), st.end());
}

bool FHEComputer::verify_proof(const std::vector<unsigned char> &proof)
{

    generate_constraints(false);

    libiop::aurora_snark_argument<FieldT, hash_type> arg;
    std::string s(proof.begin(), proof.end());
    std::istringstream iss(std::move(s));
    arg.deserialize(iss);
    return verify_argument(arg);
}

uint32_t FHEComputer::difficulty()
{
    return ast_->root_->depth_;
}

std::vector<unsigned char> FHEComputer::output()
{
    if (!last_res_)
    {
        last_res_ = eval(ast_->root_, true);
    }

    std::ostringstream oss;
    Serial::Serialize(last_res_, oss, SerType::BINARY);
    auto oss_str = oss.str();
    return std::vector<unsigned char>(oss_str.begin(), oss_str.end());
}

std::vector<unsigned char> FHEComputer::serialize(bool include_output)
{
    if (include_output)
    {
        std::vector<unsigned char> res;
        auto out = output();
        auto csize = util::uint64_to_vector_big_endian(out.size());
        res.insert(res.end(), csize.begin(), csize.end());
        res.insert(res.end(), out.begin(), out.end());
        auto compser = computation_->serialize();
        res.insert(res.end(), compser.begin(), compser.end());
        return res;
    }
    return computation_->serialize();
}

std::vector<unsigned char> FHEComputer::serialize_for_hash()
{
    return computation_->serialize();
}

std::vector<unsigned char> FHEComputer::hash()
{
    if (has_hash_)
    {
        return hash_;
    }

    auto ser = serialize_for_hash();

    hash_.resize(crypto_generichash_BYTES);
    crypto_generichash(hash_.data(), crypto_generichash_BYTES, ser.data(), ser.size(), nullptr, 0);
    has_hash_ = true;

    return hash_;
}

std::vector<unsigned char> FHEComputer::hash_force()
{
    auto ser = serialize_for_hash();

    hash_.resize(crypto_generichash_BYTES);
    crypto_generichash(hash_.data(), crypto_generichash_BYTES, ser.data(), ser.size(), nullptr, 0);
    has_hash_ = true;

    return hash_;
}

void FHEComputer::set_stop_flag(std::shared_ptr<std::atomic<bool>> stop_flag)
{
    stop_flag_ = stop_flag;
}

ProtoComputation FHEComputer::to_proto() const
{
    ProtoComputation pc;

    pc.set_expression(computation_->expression_);

    std::ostringstream pubos;
    Serial::Serialize(computation_->publicKey_, pubos, SerType::BINARY);
    auto pubos_str = pubos.str();
    pc.set_public_key(pubos_str);

    std::ostringstream evos;
    computation_->GetCryptoContext()->SerializeEvalMultKey(evos, SerType::BINARY);
    auto evos_str = evos.str();
    pc.set_evalmult_key(evos_str);

    pc.set_timestamp(computation_->timestamp_);

    // since serialization uses the unbound ciphertexts, get from archive if current ones are bound
    auto c_vec = (computation_->is_bound_) ? computation_->unbound_ciphertexts_archive_ : computation_->ciphertexts_;
    for (const auto &c : c_vec)
    {
        std::ostringstream oss;
        Serial::Serialize(c, oss, SerType::BINARY);
        auto oss_str = oss.str();
        pc.add_ciphertexts()->assign(oss_str);
    }

    if (proof_.size() > 0)
    {
        pc.set_proof(std::string(proof_.begin(), proof_.end()));
    }

    if (last_res_)
    {
        std::ostringstream oss;
        Serial::Serialize(last_res_, oss, SerType::BINARY);
        auto oss_str = oss.str();
        pc.set_output(oss_str);
    }

    return pc;
}

FHEComputer FHEComputer::from_proto(const ProtoComputation &proto)
{
    FHEComputer computer;
    computer.has_hash_ = false;
    computer.computation_ = std::make_shared<FHEComputation>(FHEComputation::from_proto(proto));
    computer.ast_ = std::make_unique<ASTree>(computer.computation_->expression_);
    if (!proto.output().empty())
    {
        std::istringstream iss(proto.output());
        Serial::Deserialize(computer.last_res_, iss, SerType::BINARY);
    }

    if (!proto.proof().empty())
    {
        computer.proof_ = std::vector<unsigned char>(proto.proof().begin(), proto.proof().end());
    }
    return computer;
}
