#ifndef DIPLO_COMPUTATION_HPP
#define DIPLO_COMPUTATION_HPP

#include <vector>
#include <cstdint>
#include <atomic>
#include <memory>

#include "message.pb.h"

class Computation
{
public:
    Computation() = default;
    // we assume serialize will contain the proof
    // TODO: maybe they should not contain the proof, and instead they
    // should contain the computation before it was bound to this block+miner
    // then, the proofs could go in the header
    virtual std::vector<unsigned char> serialize(bool include_output) = 0;
    // virtual std::vector<unsigned char> deserialize() = 0;
    virtual std::vector<unsigned char> hash() = 0;
    virtual std::vector<unsigned char> hash_force() = 0;

    virtual std::vector<unsigned char> proof() = 0;
    virtual bool verify_proof(const std::vector<unsigned char> &) = 0;
    virtual void generate_proof() = 0;
    virtual std::vector<unsigned char> output() = 0;

    virtual void bind_to_data(const std::vector<unsigned char> &) = 0;

    // virtual void deserialize(const std::vector<unsigned char> &) = 0;

    virtual uint32_t difficulty() = 0;
    virtual void set_stop_flag(std::shared_ptr<std::atomic<bool>>) = 0;

    virtual ProtoComputation to_proto() const = 0;

    virtual ~Computation() = default;
};

class ComputationFactory
{
public:
    virtual std::shared_ptr<Computation> createComputation(const ProtoComputation &proto) const = 0;
    virtual ~ComputationFactory() = default;
};

#endif