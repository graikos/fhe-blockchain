#include <iostream>
#include "computer/ast.hpp"
#include "computer/fhe_computer.hpp"

#include <fstream>

#include "nlohmann/json.hpp"

#include <atomic>

using json = nlohmann::json;

int main()
{

    std::ifstream ifs("computation.json");

    json c_json = json::parse(ifs);
    ifs.close();

    std::vector<unsigned char> seed = {0x01};

    auto stop_flag = std::make_shared<std::atomic<bool>>(false);

    FHEComputer computer(c_json);
    computer.set_stop_flag(stop_flag);

    std::cout << computer.computation_->expression_ << std::endl;

    computer.ast_->bfs_print();

    computer.bind_to_data(seed);
    auto result = computer.evaluate();
    computer.generate_constraints();
    auto arg = computer.generate_argument();
    std::cout << "Valid?: " << computer.verify_argument(arg) << std::endl;
    // computer.generate_witness();

    // load key
    std::ifstream kif("privateKey.dat", std::ios::binary);
    PrivateKey<DCRTPoly> privkey;
    Serial::Deserialize(privkey, kif, SerType::BINARY);
    kif.close();

    auto cc = computer.GetCryptoContext();

    Plaintext p;
    cc->Decrypt(privkey, result, &p);
    std::cout << p << std::endl;

    std::cout << computer.difficulty() << std::endl;

    return 0;
}