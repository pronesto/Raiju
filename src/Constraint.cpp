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

AnalyzedValue InitializationConstraint::evaluateRHS(AbstractState& A)
{
    AnalyzedValue av;
    av.addConstant(constant);
    return av;
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

AnalyzedValue PhiConstraint::evaluateRHS(AbstractState& A)
{
    AnalyzedValue result;
    for (const auto& op : operands) {
        result.join(A[op]);
    }
    return result;
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

AnalyzedValue AddConstraint::evaluateRHS(AbstractState& A)
{
    return A[op1].add(A[op2]);    
}


// --- IntersectionConstraint (v0 = v1 intersection [low, up]) ---
IntersectionConstraint::IntersectionConstraint(
    std::string dest,
    std::string src,
    IntersectionBound low,
    IntersectionBound up)
    : Constraint(std::move(dest)),
      operand(std::move(src)),
      lower_bound(std::move(low)),
      upper_bound(std::move(up))
{}

AnalyzedValue::Bound
IntersectionConstraint::resolveBound(const IntersectionBound& b,
                                     const bool isLower,
                                     const AbstractState&) const {

    if (std::holds_alternative<AnalyzedValue::Bound>(b))
        return std::get<AnalyzedValue::Bound>(b);

    // Growth phase:
    // ignore symbolic references
    AnalyzedValue::Bound result;

    if (isLower)
        result.type = AnalyzedValue::Bound::Type::MinusInfinity;
    else
        result.type = AnalyzedValue::Bound::Type::PlusInfinity;

    result.value = 0;
    return result;
}

bool IntersectionConstraint::eval(AbstractState& A) {

    AnalyzedValue oldValue = A[variable_name];
    const AnalyzedValue& src = A[operand];

    // Bottom stays bottom.
    if (src.getKind() == AnalyzedValue::Kind::Set &&
        src.getValues().empty()) {
        return false;
    }

    auto low = resolveBound(lower_bound, true, A);
    auto up  = resolveBound(upper_bound, false, A);

    AnalyzedValue result;

    // Source is a finite set.
    if (src.getKind() == AnalyzedValue::Kind::Set) {

        for (int v : src.getValues()) {
            bool keep = true;
            if (low.type == AnalyzedValue::Bound::Type::Constant)
                keep &= (v >= low.value);
            if (up.type == AnalyzedValue::Bound::Type::Constant)
                keep &= (v <= up.value);
            if (keep)
                result.addConstant(v);
        }
    }

    // Source is already an interval.
    else {
        auto lower = src.getLower();
        auto upper = src.getUpper();

        // max(lower, low)
        if (low.type == AnalyzedValue::Bound::Type::Constant) {
            if (lower.type == AnalyzedValue::Bound::Type::MinusInfinity)
                lower = low;
            else if (lower.type == AnalyzedValue::Bound::Type::Constant)
                lower.value = std::max(lower.value, low.value);
        }

        // min(upper, up)
        if (up.type == AnalyzedValue::Bound::Type::Constant) {
            if (upper.type == AnalyzedValue::Bound::Type::PlusInfinity)
                upper = up;
            else if (upper.type == AnalyzedValue::Bound::Type::Constant)
                upper.value = std::min(upper.value, up.value);
        }

        // Empty interval?
        if (lower.type == AnalyzedValue::Bound::Type::Constant &&
            upper.type == AnalyzedValue::Bound::Type::Constant &&
            lower.value > upper.value) {
            // Leave result as bottom.
        }
        else {
            result.setAsInterval(lower, upper, src.getStride());
        }
    }

    A[variable_name] = result;
    return oldValue != result;
}

AnalyzedValue IntersectionConstraint::evaluateRHS(AbstractState& A)
{
    AnalyzedValue::Bound low = resolveBound(lower_bound, true, A);
    AnalyzedValue::Bound up = resolveBound(upper_bound, false, A);
    
    AnalyzedValue result;
    result.setAsInterval(low, up);
    return result.intersect(A[operand]);
}
