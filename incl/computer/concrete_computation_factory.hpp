#ifndef DIPLO_CONCRETE_COMPUTATION_FACTORY
#define DIPLO_CONCRETE_COMPUTATION_FACTORY

#include "core/block_header.hpp"

#include "message.pb.h"

#include "computer/fhe_computer.hpp"

#include <memory>

class ConcreteComputationFactory : public ComputationFactory {
public:
    std::shared_ptr<Computation> createComputation(const ProtoComputation &proto) const override;
};

#endif
