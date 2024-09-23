#include "libsnark/relations/variable.hpp"
#include "libsnark/relations/constraint_satisfaction_problems/r1cs/r1cs.hpp"
#include "libiop/relations/r1cs.hpp"
#include "libiop/relations/variable.hpp"

template <typename FieldT>
void libsnark_to_libiop_linear_term(const libsnark::linear_term<FieldT> &ls_lt, libiop::linear_term<FieldT> &liop_lt)
{
    liop_lt.index_ = ls_lt.index;
    liop_lt.coeff_ = ls_lt.coeff;
}

template <typename FieldT>
void libsnark_to_libiop_linear_combination(const libsnark::linear_combination<FieldT> &ls_lc, libiop::linear_combination<FieldT> &liop_lc)
{
    for (const auto &p : ls_lc.terms)
    {
        libiop::linear_term<FieldT> new_liop_term;
        libsnark_to_libiop_linear_term(p, new_liop_term);
        liop_lc.add_term(new_liop_term);
    }
}

template <typename FieldT>
void libsnark_to_libiop_r1cs_constraint(const libsnark::r1cs_constraint<FieldT> &ls_cons, libiop::r1cs_constraint<FieldT> &liop_cons)
{
    libsnark_to_libiop_linear_combination(ls_cons.a, liop_cons.a_);
    libsnark_to_libiop_linear_combination(ls_cons.b, liop_cons.b_);
    libsnark_to_libiop_linear_combination(ls_cons.c, liop_cons.c_);
}

template <typename FieldT>
void libsnark_to_libiop_r1cs_constraint_system(const libsnark::r1cs_constraint_system<FieldT> &ls_cons_system, libiop::r1cs_constraint_system<FieldT> &liop_const_system)
{
    liop_const_system.primary_input_size_ = ls_cons_system.primary_input_size;
    liop_const_system.auxiliary_input_size_ = ls_cons_system.auxiliary_input_size;
    for (const auto &c : ls_cons_system.constraints)
    {
        libiop::r1cs_constraint<FieldT> new_cons;
        libsnark_to_libiop_r1cs_constraint(c, new_cons);
        liop_const_system.add_constraint(new_cons);
    }
}

template <typename FieldT>
void pad_constraints_to_next_power_of_two(libiop::r1cs_constraint_system<FieldT> &cs)
{
    auto num_constraints = cs.num_constraints();
    const size_t next_power_of_two = 1ull << libff::log2(num_constraints);
    const size_t pad_amount = next_power_of_two - num_constraints;
    libiop::linear_combination<FieldT> a_zero(0);
    libiop::linear_combination<FieldT> b_zero(0);
    libiop::linear_combination<FieldT> c_zero(0);
    for (size_t i = 0; i < pad_amount; ++i)
    {
        // inside the constructor, the linear combination objects are copied, so we can use the same for every loop
        libiop::r1cs_constraint<FieldT> new_c(a_zero, b_zero, c_zero);
        cs.add_constraint(new_c);
    }
}

template <typename FieldT>
void pad_constraint_system_for_aurora(libiop::r1cs_constraint_system<FieldT> &cs)
{

    // compute padding needed for inputs
    const size_t old_primary_input_size = cs.num_inputs();
    const size_t input_next_power_of_two = 1ull << libff::log2(old_primary_input_size + 1);
    const size_t input_pad_amount = input_next_power_of_two - (old_primary_input_size + 1);

    if (input_pad_amount > 0)
    {
        // update info
        cs.primary_input_size_ += input_pad_amount;

        // transform constraints when indices are affected
        // the 0-th element (constant 1) is not included in num_variables, hence not in primary input size
        // e.g.
        // old input size: 4, pad to: 7, padding no: 3
        // lt index: 0 NOT AFFECTED
        // lt index: 1 NOT AFFECTED
        // lt index: 2 NOT AFFECTED
        // lt index: 3 NOT AFFECTED
        // lt index: 4 NOT AFFECTED
        // lt index: 5 AFFECTED, moved to 5+3
        // Meaning, condition of transformation should be that every lt with index > old input size, mapped to index+pad
        for (auto &c : cs.constraints_)
        {
            for (auto &lt : c.a_.terms)
            {
                if (lt.index_ > old_primary_input_size)
                {
                    lt.index_ += input_pad_amount;
                }
            }
            for (auto &lt : c.b_.terms)
            {
                if (lt.index_ > old_primary_input_size)
                {
                    lt.index_ += input_pad_amount;
                }
            }
            for (auto &lt : c.c_.terms)
            {
                if (lt.index_ > old_primary_input_size)
                {
                    lt.index_ += input_pad_amount;
                }
            }
        }
    }

    // pad with constraints. This does not affect anything else. Done here to avoid iterating through
    // the padding when adding primary input pad
    pad_constraints_to_next_power_of_two(cs);

    auto num_variables = cs.num_variables();
    const size_t var_next_power_of_two = 1ull << libff::log2(num_variables + 1);
    const size_t var_pad_amount = var_next_power_of_two - (num_variables + 1);
    // this time, to reach the desired variable size, padding will be added to the auxiliary input
    cs.auxiliary_input_size_ += var_pad_amount;
}

template <typename FieldT>
void pad_primary_input_to_match_cs(libiop::r1cs_constraint_system<FieldT> &cs, libiop::r1cs_primary_input<FieldT> &prim)
{
    auto left = cs.num_inputs() - prim.size();
    while (left > 0)
    {
        prim.push_back(FieldT(0));
        --left;
    }
}

template <typename FieldT>
void pad_auxiliary_input_to_match_cs(libiop::r1cs_constraint_system<FieldT> &cs, libiop::r1cs_auxiliary_input<FieldT> &aux)
{
    auto left = cs.auxiliary_input_size_ - aux.size();
    while (left > 0)
    {
        aux.push_back(FieldT(0));
        --left;
    }
}

/*
Requirements:
- Number of constraints to be a power of 2
- Number of variables to be a power of 2 minus 1
- Number of inputs to be a power of 2 minus 1

STEPS:
1. Add empty constraints till power of 2. This does not affect anything
2. Compute next power of 2 (-1) for input size. Then, make the primary_input_size equal to this.
3. For every constraint, check if var_index is greater than prev primary_input_size. If yes, offset with the padding added
4. Compute next power of 2 (-1) for total variable size. Then, add auxiliary variables equal to 0 to reach this number
*/