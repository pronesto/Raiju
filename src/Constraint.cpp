/**
 * @file Constraint.cpp
 * @brief Concrete implementations of constraint evaluation routines.
 */

#include "Constraint.h"
#include <algorithm>
#include <cmath>

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

// --- ArithmeticConstraint Base ---
ArithmeticConstraint::ArithmeticConstraint(std::string dest, std::string lhs, std::string rhs)
    : Constraint(std::move(dest)), op1(std::move(lhs)), op2(std::move(rhs)) {}

// --- AddConstraint (v0 = v1 + v2) ---
bool AddConstraint::eval(AbstractState& A) {
    AnalyzedValue old_val = A[variable_name];
    
    const AnalyzedValue& lhs = A[op1];
    const AnalyzedValue& rhs = A[op2];
    
    AnalyzedValue result; // Starts at bottom (empty set)

    // Case 1: Both are explicit sets -> compute exact pairwise additions
    if (lhs.getKind() == AnalyzedValue::Kind::Set && rhs.getKind() == AnalyzedValue::Kind::Set) {
        // If either set is completely empty (bottom), the result remains bottom
        if (lhs.getValues().empty() || rhs.getValues().empty()) {
            A[variable_name] = result;
            return old_val != result;
        }

        // Add all cross-combinations of exact values
        for (int l : lhs.getValues()) {
            for (int r : rhs.getValues()) {
                result.addConstant(l + r); 
                // Note: result.addConstant will automatically collapse it 
                // into a StridedInterval if it crosses threshold N
            }
        }
    } 
    // Case 2: At least one is a Strided Interval -> compute interval addition hull
    else {
        // Force evaluation via structural intervals
        // Lower bound addition
        AnalyzedValue::Bound new_low;
        if (lhs.getLower().type == AnalyzedValue::Bound::Type::MinusInfinity || 
            rhs.getLower().type == AnalyzedValue::Bound::Type::MinusInfinity) {
            new_low.type = AnalyzedValue::Bound::Type::MinusInfinity;
            new_low.value = 0;
        } else {
            new_low.type = AnalyzedValue::Bound::Type::Constant;
            new_low.value = lhs.getLower().value + rhs.getLower().value;
        }

        // Upper bound addition
        AnalyzedValue::Bound new_up;
        if (lhs.getUpper().type == AnalyzedValue::Bound::Type::PlusInfinity || 
            rhs.getUpper().type == AnalyzedValue::Bound::Type::PlusInfinity) {
            new_up.type = AnalyzedValue::Bound::Type::PlusInfinity;
            new_up.value = 0;
        } else {
            new_up.type = AnalyzedValue::Bound::Type::Constant;
            new_up.value = lhs.getUpper().value + rhs.getUpper().value;
        }

        // For strides, adding two strided sequences yields a stride of gcd(s1, s2)
        unsigned s1 = (lhs.getKind() == AnalyzedValue::Kind::StridedInterval) ? lhs.getStride() : 1;
        unsigned s2 = (rhs.getKind() == AnalyzedValue::Kind::StridedInterval) ? rhs.getStride() : 1;
        unsigned new_stride = std::gcd(s1, s2);

        result.setAsInterval(new_low, new_up, new_stride); 
    }

    A[variable_name] = result;
    return old_val != result;
}


// --- IntersectionConstraint (v0 = v1 intersection [low, up]) ---
IntersectionConstraint::IntersectionConstraint(std::string dest, std::string src, 
                                               IntersectionBound low, IntersectionBound up)
    : Constraint(std::move(dest)), operand(std::move(src)), 
      lower_bound(std::move(low)), upper_bound(std::move(up)) {}

AnalyzedValue::Bound IntersectionConstraint::resolveBound(const IntersectionBound& b, const AbstractState& A) const {
    if (std::holds_alternative<AnalyzedValue::Bound>(b)) {
        return std::get<AnalyzedValue::Bound>(b);
    }
    
    // Resolve Future references dynamically from current state table
    const Future& fut = std::get<Future>(b);
    auto it = A.find(fut.target_variable);
    if (it == A.end()) {
        // Target not initialized yet, fallback safely
        return AnalyzedValue::Bound{AnalyzedValue::Bound::Type::Constant, 0};
    }

    const AnalyzedValue& target_val = it->second;
    AnalyzedValue::Bound resolved;
    
    // Futures look at corresponding edge limits depending on whether it's an upper or lower context.
    // For simplicity, let's assume it checks the target variable's constant values.
    if (target_val.getKind() == AnalyzedValue::Kind::Set && !target_val.getValues().empty()) {
        resolved.type = AnalyzedValue::Bound::Type::Constant;
        // If resolving for lower vs upper, take min or max. Let's adapt with an offset.
        resolved.value = target_val.getValues().front() + fut.offset;
    } else {
        // If it's an interval, extract bounds safely
        resolved = target_val.getLower(); // fallback structure
        if (resolved.type == AnalyzedValue::Bound::Type::Constant) {
            resolved.value += fut.offset;
        }
    }
    return resolved;
}

bool IntersectionConstraint::eval(AbstractState& A) {
    AnalyzedValue old_val = A[variable_name];
    const AnalyzedValue& src_val = A[operand];
    
    AnalyzedValue::Bound resolved_low = resolveBound(lower_bound, A);
    AnalyzedValue::Bound resolved_up = resolveBound(upper_bound, A);

    AnalyzedValue result; // Starts empty

    // Slice or narrow down values based on incoming rules
    if (src_val.getKind() == AnalyzedValue::Kind::Set) {
        for (int v : src_val.getValues()) {
            bool low_ok = (resolved_low.type == AnalyzedValue::Bound::Type::MinusInfinity) || (v >= resolved_low.value);
            bool up_ok = (resolved_up.type == AnalyzedValue::Bound::Type::PlusInfinity) || (v <= resolved_up.value);
            if (low_ok && up_ok) {
                result.addConstant(v);
            }
        }
    } else {
        // Structural Interval intersection
        // Lower limit narrowing
        AnalyzedValue::Bound final_low = src_val.getLower();
        if (resolved_low.type == AnalyzedValue::Bound::Type::Constant) {
            if (final_low.type == AnalyzedValue::Bound::Type::MinusInfinity || final_low.value < resolved_low.value) {
                final_low = resolved_low;
            }
        }
        
        // Upper limit narrowing
        AnalyzedValue::Bound final_up = src_val.getUpper();
        if (resolved_up.type == AnalyzedValue::Bound::Type::Constant) {
            if (final_up.type == AnalyzedValue::Bound::Type::PlusInfinity || final_up.value > resolved_up.value) {
                final_up = resolved_up;
            }
        }
        
        result.setAsInterval(final_low, final_up, src_val.getStride());
    }

    A[variable_name] = result;
    return old_val != result;
}
