#include "openfhe.h"
#include "proofsystem/proofsystem_libsnark.h"
#include <iostream>
#include "proofsystem/gadgets_libsnark.h"

#include "conv/conv.hpp"

#include <libff/algebra/fields/binary/gf64.hpp>
#include <libff/algebra/curves/edwards/edwards_pp.hpp>
#include "libiop/snark/aurora_snark.hpp"
#include "libiop/relations/examples/r1cs_examples.hpp"

using std::cout, std::endl;
using namespace lbcrypto;

int main() {
    libff::default_ec_pp::init_public_params();

    // Sample Program: Step 1 - Set CryptoContext
    CCParams<CryptoContextBGVRNS> parameters;
    parameters.SetMultiplicativeDepth(1);
    parameters.SetPlaintextModulus(65537);
    parameters.SetScalingTechnique(FIXEDMANUAL);
    parameters.SetKeySwitchTechnique(
        KeySwitchTechnique::
            BV);  // use BV instead of HYBRID, as it is a lot simpler to arithmetize, even if it requires a quadratic number of NTTs

    CryptoContext<DCRTPoly> cryptoContext = GenCryptoContext(parameters);
    // Enable features that you wish to use
    cryptoContext->Enable(PKE);
    cryptoContext->Enable(KEYSWITCH);
    cryptoContext->Enable(LEVELEDSHE);
    EvalKey<DCRTPoly> x;

    // Sample Program: Step 2 - Key Generation

    // Initialize Public Key Containers
    KeyPair<DCRTPoly> keyPair;

    // Generate a public/private key pair
    keyPair = cryptoContext->KeyGen();

    // Generate the relinearization key
    cryptoContext->EvalMultKeyGen(keyPair.secretKey);

    // Generate the rotation evaluation keys
    cryptoContext->EvalRotateKeyGen(keyPair.secretKey, {1, 2, -1, -2});

    // Print parameters
    cout << "N: " << cryptoContext->GetRingDimension() << endl;
    cout << "t: " << cryptoContext->GetCryptoParameters()->GetPlaintextModulus() << endl;
    cout << "Q: " << cryptoContext->GetCryptoParameters()->GetElementParams()->GetModulus() << endl;
    cout << "KeySwitchTechnique: " << parameters.GetKeySwitchTechnique() << endl;
    cout << "DigitSize: " << parameters.GetDigitSize() << endl;

	   // First plaintext vector is encoded
    std::vector<int64_t> vectorOfInts1 = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    Plaintext plaintext1               = cryptoContext->MakePackedPlaintext(vectorOfInts1);
    // Second plaintext vector is encoded
    std::vector<int64_t> vectorOfInts2 = {3, 2, 1, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    Plaintext plaintext2               = cryptoContext->MakePackedPlaintext(vectorOfInts2);
    // Third plaintext vector is encoded
    std::vector<int64_t> vectorOfInts3 = {1, 2, 5, 2, 5, 6, 7, 8, 9, 10, 11, 12};
    Plaintext plaintext3               = cryptoContext->MakePackedPlaintext(vectorOfInts3);

    // The encoded vectors are encrypted
    auto ciphertext1 = cryptoContext->Encrypt(keyPair.publicKey, plaintext1);
    auto ciphertext2 = cryptoContext->Encrypt(keyPair.publicKey, plaintext2);
    auto ciphertext3 = cryptoContext->Encrypt(keyPair.publicKey, plaintext3);

    cout << "Encryption: done" << endl;

    // Sample Program: Step 4 - Evaluation

    Ciphertext<DCRTPoly> ctxt_out;

    // // Homomorphic additions
    // auto ciphertextAdd12 = cryptoContext->EvalAdd(ciphertext1, ciphertext3);
    std::cout << "after cc add" << std::endl;
    // auto ciphertextSub12 = cryptoContext->EvalAdd(ciphertext1, ciphertext2);
    std::cout << "after cc sub" << std::endl;

    // auto ciphertextRot = cryptoContext->EvalRotate(ciphertext1, 1);
    // std::cout << "after cc rot" << std::endl;

    auto ciphertextMul = cryptoContext->EvalMultNoRelin(ciphertext1, ciphertext2);
    std::cout << "after cc mult norelin" << std::endl;

    // auto ciphertextRelin = cryptoContext->Relinearize(ciphertextMul);
    std::cout << "after cc relin" << std::endl;

    ////

    LibsnarkProofSystem ps(cryptoContext);
    ps.SetMode(PROOFSYSTEM_MODE::PROOFSYSTEM_MODE_CONSTRAINT_GENERATION);

    ps.PublicInput(ciphertext1);
    ps.PublicInput(ciphertext2);
    // ps.PublicInput(ciphertext3);
    // TODO important: only add public input before adding any other constraints!
    auto vars_out = *ps.ConstrainPublicOutput(ciphertextMul);

    std::cout << "Example thread id: " << std::this_thread::get_id() << std::endl;
    // ciphertextAdd12 = ps.EvalAdd(ciphertext1, ciphertext2);
    std::cout << "After eval add" << std::endl;
    // ciphertextSub12 = ps.EvalAdd(ciphertext1, ciphertext2);
    std::cout << "After eval sub" << std::endl;

    ciphertextMul = ps.EvalMultNoRelin(ciphertext1, ciphertext2);
    std::cout << "After eval mult" << std::endl;
    ps.FinalizeOutputConstraints(ciphertextMul, vars_out);
    // ps.Relinearize(ciphertextMul);
    // std::cout << "After relin" << std::endl;
    // std::cout << "After finalize" << std::endl;

    // std::cout << "now witness" << std::endl;
    // ps.SetMode(PROOFSYSTEM_MODE::PROOFSYSTEM_MODE_WITNESS_GENERATION);
    // ps.PublicInput(ciphertext1);
    // ps.PublicInput(ciphertext2);
    // ciphertextMul = ps.EvalMultNoRelin(ciphertext1, ciphertext2);
    // std::cout << "before finalize" << std::endl;




	const auto pb                                          = ps.pb;
    const r1cs_constraint_system<FieldT> constraint_system = ps.pb.get_constraint_system();

    cout << "#inputs:      " << constraint_system.num_inputs() << endl;
    cout << "#variables:   " << constraint_system.num_variables() << endl;
    cout << "#constraints: " << constraint_system.num_constraints() << endl;

    bool satisfied = constraint_system.is_satisfied(pb.primary_input(), pb.auxiliary_input());
    cout << "satisfied:    " << std::boolalpha << satisfied << endl;
    









    return 0;






    libiop::r1cs_constraint_system<FieldT> liop_cs;
    libsnark_to_libiop_r1cs_constraint_system(constraint_system, liop_cs);
    std::cout << "Just converted to libiop constraint system:" << std::endl;
    cout << "#inputs:      " << liop_cs.num_inputs() << endl;
    cout << "#variables:   " << liop_cs.num_variables() << endl;
    cout << "#constraints: " << liop_cs.num_constraints() << endl;

    pad_constraint_system_for_aurora(liop_cs);
    std::cout << "Padding libiop constraint system:" << std::endl;
    cout << "#inputs:      " << liop_cs.num_inputs() << endl;
    cout << "#variables:   " << liop_cs.num_variables() << endl;
    cout << "#constraints: " << liop_cs.num_constraints() << endl;


	std::cout << "============================" << std::endl;
	std::cout << "============================" << std::endl;
	std::cout << "NOW AURORA" << std::endl;
	std::cout << "============================" << std::endl;
	std::cout << "============================" << std::endl;

	 /* Set up R1CS */
    // typedef libff_liop::gf64 FieldT;
    typedef libiop::binary_hash_digest hash_type;

    // const std::size_t num_constraints = 1 << 13;
    // const std::size_t num_inputs = (1 << 5) - 1;
    // const std::size_t num_variables = (1 << 13) - 1;
    const size_t security_parameter = 128;
    const size_t RS_extra_dimensions = 2;
    const size_t FRI_localization_parameter = 3;
    const libiop::LDT_reducer_soundness_type ldt_reducer_soundness_type = libiop::LDT_reducer_soundness_type::optimistic_heuristic;
    const libiop::FRI_soundness_type fri_soundness_type = libiop::FRI_soundness_type::heuristic;
    const libiop::field_subset_type domain_type = libiop::affine_subspace_type;

    // libiop::r1cs_example<FieldT> r1cs_params = libiop::generate_r1cs_example<FieldT>(
    //     num_constraints, num_inputs, num_variables);

    libiop::r1cs_primary_input<FieldT> cs_primary_input = pb.primary_input();
    libiop::r1cs_auxiliary_input<FieldT> cs_auxiliary_input = pb.auxiliary_input();

    pad_primary_input_to_match_cs(liop_cs, cs_primary_input);
    pad_auxiliary_input_to_match_cs(liop_cs, cs_auxiliary_input);


    /* Actual SNARK test */
    for (std::size_t i = 0; i < 2; i++) {
        const bool make_zk = (i == 0) ? false : true;
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
        // const libiop::aurora_snark_argument<FieldT, hash_type> argument = aurora_snark_prover<FieldT>(
        //     r1cs_params.constraint_system_,
        //     r1cs_params.primary_input_,
        //     r1cs_params.auxiliary_input_,
        //     params);
        std::cout << "reach" << std::endl;

        const libiop::aurora_snark_argument<FieldT, hash_type> argument = aurora_snark_prover<FieldT>(
            liop_cs,
            cs_primary_input,
            cs_auxiliary_input,
            params);

        printf("iop size in bytes %lu\n", argument.IOP_size_in_bytes());
        printf("bcs size in bytes %lu\n", argument.BCS_size_in_bytes());
        printf("argument size in bytes %lu\n", argument.size_in_bytes());

        const bool bit = aurora_snark_verifier<FieldT, hash_type>(
            liop_cs,
            cs_primary_input,
            argument,
            params);


		std::cout << "Bit is: " << bit << std::endl;
    }


    return 0;
}