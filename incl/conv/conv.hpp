#ifndef DIPLO_CONV_HPP
#define DIPLO_CONV_HPP

#include "libsnark/relations/variable.hpp"
#include "libsnark/relations/constraint_satisfaction_problems/r1cs/r1cs.hpp"
#include "libiop/relations/r1cs.hpp"
#include "libiop/relations/variable.hpp"

template <typename FieldT>
void libsnark_to_libiop_linear_term(const libsnark::linear_term<FieldT> &ls_lt, libiop::linear_term<FieldT> &liop_lt);

template <typename FieldT>
void libsnark_to_libiop_linear_combination(const libsnark::linear_combination<FieldT> &ls_lc, libiop::linear_combination<FieldT> &liop_lc);

template <typename FieldT>
void libsnark_to_libiop_r1cs_constraint(const libsnark::r1cs_constraint<FieldT> &ls_cons, libiop::r1cs_constraint<FieldT> &liop_cons);

template <typename FieldT>
void libsnark_to_libiop_r1cs_constraint_system(const libsnark::r1cs_constraint_system<FieldT> &ls_cons_system, libiop::r1cs_constraint_system<FieldT> &liop_const_system);

template <typename FieldT>
void pad_constraints_to_next_power_of_two(libiop::r1cs_constraint_system<FieldT> &cs);

template <typename FieldT>
void pad_constraint_system_for_aurora(libiop::r1cs_constraint_system<FieldT> &cs);

template <typename FieldT>
void pad_primary_input_to_match_cs(libiop::r1cs_constraint_system<FieldT> &cs, libiop::r1cs_primary_input<FieldT> &prim);

template <typename FieldT>
void pad_auxiliary_input_to_match_cs(libiop::r1cs_constraint_system<FieldT> &cs, libiop::r1cs_auxiliary_input<FieldT> &aux);

#include "conv.tcc"

#endif