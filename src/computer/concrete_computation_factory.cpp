#include "computer/concrete_computation_factory.hpp"

std::shared_ptr<Computation> ConcreteComputationFactory::createComputation(const ProtoComputation &proto) const
{
    return std::make_shared<FHEComputer>(FHEComputer::from_proto(proto));
}