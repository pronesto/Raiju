/**
 * @file Constraint.cpp
 * @brief Concrete implementations of constraint evaluation routines.
 */

#include "Constraint.h"
#include <algorithm>

// --- Base Constraint ---
Constraint::Constraint(std::string name) : variable_name(std::move(name)) {}

// --- InitializationConstraint (v = c) ---
InitializationConstraint::InitializationConstraint(std::string var, int c)
    : Constraint(std::move(var)), constant(c) {}

bool InitializationConstraint::eval(AbstractState& A) {
    AnalyzedValue old_val = A[variable_name];

    // Build the exact set representation {c}
    AnalyzedValue new_val;
    new_val.addConstant(constant);

    A[variable_name] = new_val;
    return old_val != new_val; // Leverages your custom equality operator!
}

// --- PhiConstraint (v0 = phi(v1, v2, ...)) ---
PhiConstraint::PhiConstraint(std::string var, std::vector<std::string> ops)
    : Constraint(std::move(var)), operands(std::move(ops)) {}

bool PhiConstraint::eval(AbstractState& A) {
    AnalyzedValue old_val = A[variable_name];
    AnalyzedValue accumulated_join; // Starts at bottom element

    for (const auto& op : operands) {
        accumulated_join.join(A[op]);
    }

    A[variable_name] = accumulated_join;
    return old_val != accumulated_join;
}
