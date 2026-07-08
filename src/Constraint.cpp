/**
 * @file Constraint.cpp
 * @brief Concrete implementations of constraint evaluation routines.
 */

#include "Constraint.h"
#include <algorithm>
#include <cmath>
#include <cassert>

// --- Base Constraint ---
Constraint::Constraint(std::string name) : def(std::move(name)) {}

// --- InitializationConstraint (v = c) ---
InitializationConstraint::InitializationConstraint(std::string var, int c)
    : Constraint(std::move(var)), constant(c) {}

bool InitializationConstraint::eval(AbstractState& A) {
    AnalyzedValue old_val = A[def];

  // Build the exact set representation {c}
  AnalyzedValue new_val;
  new_val.addConstant(constant);

    A[def] = new_val;
    return old_val != new_val; // Leverages your custom equality operator!
}

// --- PhiConstraint (v0 = phi(v1, v2, ...)) ---
PhiConstraint::PhiConstraint(std::string var, std::vector<std::string> ops)
    : Constraint(std::move(var)), operands(std::move(ops)) {}

bool PhiConstraint::eval(AbstractState& A) {
    AnalyzedValue old_val = A[def];
    AnalyzedValue accumulated_join; // Starts at bottom element

  for (const auto &op : operands) {
    accumulated_join.join(A[op]);
  }

    A[def] = accumulated_join;
    return old_val != accumulated_join;
}

// --- ArithmeticConstraint Base ---
ArithmeticConstraint::ArithmeticConstraint(std::string dest, std::string lhs,
                                           std::string rhs)
    : Constraint(std::move(dest)), op1(std::move(lhs)), op2(std::move(rhs)) {}

bool AddConstraint::eval(AbstractState& A) {
    AnalyzedValue old_val = A[def];
    
    const AnalyzedValue &lhs = A[op1];
    const AnalyzedValue &rhs = A[op2];
    
    AnalyzedValue result; // Starts at bottom (empty set)

    // Exact evaluation: finite set × finite set
    if (lhs.getKind() == AnalyzedValue::Kind::Set &&
        rhs.getKind() == AnalyzedValue::Kind::Set) {

        // Bottom propagates.
        if (lhs.getValues().empty() || rhs.getValues().empty()) {
            A[def] = result;
            return old_val != result;
        }

        std::vector<int> consts;

        for (int l : lhs.getValues()) {
            for (int r : rhs.getValues()) {
                consts.emplace_back(l + r);
            }
        }

        result.addConstants(consts);

    A[def] = result;
    return old_val != result;
  }

  // Otherwise treat every operand as a strided interval.
  auto getLower = [](const AnalyzedValue &v) -> AnalyzedValue::Bound {
    if (v.getKind() == AnalyzedValue::Kind::Set) {
      return {AnalyzedValue::Bound::Type::Constant,
              *v.getValues().begin()};
    }
    return v.getLower();
  };

  auto getUpper = [](const AnalyzedValue &v) -> AnalyzedValue::Bound {
    if (v.getKind() == AnalyzedValue::Kind::Set) {
      return {AnalyzedValue::Bound::Type::Constant,
              *v.getValues().rbegin()};
    }
    return v.getUpper();
  };

  auto addLower =
      [](const AnalyzedValue::Bound &a,
         const AnalyzedValue::Bound &b) -> AnalyzedValue::Bound {

    if (a.type == AnalyzedValue::Bound::Type::MinusInfinity ||
        b.type == AnalyzedValue::Bound::Type::MinusInfinity)
      return {AnalyzedValue::Bound::Type::MinusInfinity, 0};

    return {AnalyzedValue::Bound::Type::Constant,
            a.value + b.value};
  };

  auto addUpper =
      [](const AnalyzedValue::Bound &a,
         const AnalyzedValue::Bound &b) -> AnalyzedValue::Bound {

    if (a.type == AnalyzedValue::Bound::Type::PlusInfinity ||
        b.type == AnalyzedValue::Bound::Type::PlusInfinity)
      return {AnalyzedValue::Bound::Type::PlusInfinity, 0};

    return {AnalyzedValue::Bound::Type::Constant,
            a.value + b.value};
  };

  auto lower = addLower(getLower(lhs), getLower(rhs));
  auto upper = addUpper(getUpper(lhs), getUpper(rhs));

  unsigned s1 =
      (lhs.getKind() == AnalyzedValue::Kind::StridedInterval)
          ? lhs.getStride()
          : 1;

  unsigned s2 =
      (rhs.getKind() == AnalyzedValue::Kind::StridedInterval)
          ? rhs.getStride()
          : 1;

  result.setAsInterval(lower, upper, std::gcd(s1, s2));

  A[def] = result;
  return old_val != result;
}

// --- IntersectionConstraint (v0 = v1 intersection [low, up]) ---
IntersectionConstraint::IntersectionConstraint(std::string dest,
                                               std::string src,
                                               IntersectionBound low,
                                               IntersectionBound up)
    : Constraint(std::move(dest)), operand(std::move(src)),
      lower_bound(std::move(low)), upper_bound(std::move(up)) {}

AnalyzedValue::Bound
IntersectionConstraint::resolveBound(const IntersectionBound &b,
                                     const bool isLower,
                                     const AbstractState &) const {

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

IntersectionConstraint
IntersectionConstraint::resolveFutures(const AbstractState &state) const {
  auto resolve = [&](const IntersectionBound &bound,
      bool isLower) -> IntersectionBound {
    if (std::holds_alternative<AnalyzedValue::Bound>(bound))
      return bound;

    const Future &future = std::get<Future>(bound);

    auto it = state.find(future.target_variable);
    assert(it != state.end());

    AnalyzedValue::Bound result =
      isLower ? it->second.getLower()
      : it->second.getUpper();

    if (result.type == AnalyzedValue::Bound::Type::Constant)
      result.value += future.offset;

    return result;
  };

  return IntersectionConstraint(
      def,
      operand,
      resolve(lower_bound, true),
      resolve(upper_bound, false));
}

bool IntersectionConstraint::eval(AbstractState &A) {

    AnalyzedValue oldValue = A[def];
    const AnalyzedValue& src = A[operand];

  // Bottom stays bottom.
  if (src.getKind() == AnalyzedValue::Kind::Set && src.getValues().empty()) {
    return false;
  }

  auto low = resolveBound(lower_bound, true, A);
  auto up = resolveBound(upper_bound, false, A);

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
    } else {
      result.setAsInterval(lower, upper, src.getStride());
    }
  }

    A[def] = result;
    return oldValue != result;
}
