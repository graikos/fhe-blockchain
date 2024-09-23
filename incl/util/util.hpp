#ifndef DIPLO_UTIL_HPP
#define DIPLO_UTIL_HPP

#include <vector>
#include <utility>
#include <charconv>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace util
{
    // Using these specific util functions instead of memcpy and casting to ensure endianness
    int chars_to_int32_big_endian(const unsigned char *buffer);
    std::vector<unsigned char> int32_to_vector_big_endian(int num);
    void int32_to_uchar_big_endian(int num, unsigned char *res);

    uint32_t chars_to_uint32_big_endian(const unsigned char *buffer);
    std::vector<unsigned char> uint32_to_vector_big_endian(uint32_t num);
    void uint32_to_uchar_big_endian(uint32_t num, unsigned char *res);

    uint64_t chars_to_uint64_big_endian(const unsigned char *buffer);
    std::vector<unsigned char> uint64_to_vector_big_endian(uint64_t num);
    void uint64_to_uchar_big_endian(uint64_t num, unsigned char *res);

    std::vector<std::pair<std::string, int>> ipv4_addr_list_from_txt(std::string &fname);
    std::vector<std::pair<std::string, int>> bootstrap_addr_from_json(const json &json_ob);

    std::string addr_and_port_to_str(const std::string &addr, int port);

    void print_hex(const std::vector<unsigned char> &vec);

    std::string txid_vout_pair_to_key(const std::vector<unsigned char> &txid, uint64_t vout);
};

#endif