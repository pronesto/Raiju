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

    std::vector<int> vals = {constant};
    A[def].addConstant(vals);
    return old_val != A[def]; // Leverages your custom equality operator!
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
    
    // AnalyzedValue result; // Starts at bottom (empty set)

    // Exact evaluation: finite set × finite set
    if (lhs.getKind() == AnalyzedValue::Kind::Set &&
        rhs.getKind() == AnalyzedValue::Kind::Set) {

        // Bottom propagates.
        if (lhs.getValues().empty() || rhs.getValues().empty()) {
            A[def].setAsBottom();
            return old_val != A[def];
        }

        std::vector<int> consts;

        for (int l : lhs.getValues()) {
            for (int r : rhs.getValues()) {
                consts.emplace_back(l + r);
            }
        }

        A[def].addConstant(consts);

    return old_val != A[def];
  }

  // Otherwise treat every operand as a strided interval.
  auto getLower = [](const AnalyzedValue &v) -> Bound {
    if (v.getKind() == AnalyzedValue::Kind::Set) {
      return Bound::constant(*v.getValues().begin());
    }
    return v.getLower();
  };

  auto getUpper = [](const AnalyzedValue &v) -> Bound {
    if (v.getKind() == AnalyzedValue::Kind::Set) {
      if (v.getValues().empty())
        return Bound::constant(*v.getValues().begin());
              
      return Bound::constant(*v.getValues().rbegin());
    }
    return v.getUpper();
  };

  auto addLower =
      [](const Bound &a,
         const Bound &b) -> Bound {

    if (a.type == Bound::Type::MinusInfinity ||
        b.type == Bound::Type::MinusInfinity)
      return Bound::minusInfinity();

    return Bound::constant(a.getConstant() + b.getConstant());
  };

  auto addUpper =
      [](const Bound &a,
         const Bound &b) -> Bound {

    if (a.type == Bound::Type::PlusInfinity ||
        b.type == Bound::Type::PlusInfinity)
      return Bound::plusInfinity();

    return Bound::constant(a.getConstant() + b.getConstant());
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

  A[def].setAsInterval(lower, upper, std::gcd(s1, s2));

  return old_val != A[def];
}

// --- IntersectionConstraint (v0 = v1 intersection [low, up]) ---
IntersectionConstraint::IntersectionConstraint(std::string dest,
                                               std::string src,
                                               IntersectionBound low,
                                               IntersectionBound up)
    : Constraint(std::move(dest)), operand(std::move(src)),
      lower_bound(std::move(low)), upper_bound(std::move(up)) {}

Bound
IntersectionConstraint::resolveBound(const IntersectionBound &b,
                                     const bool isLower,
                                     const AbstractState &) const {

  if (std::holds_alternative<Bound>(b))
    return std::get<Bound>(b);

  // Growth phase:
  // ignore symbolic references
  Bound result;

  if (isLower)
    result.type = Bound::Type::MinusInfinity;
  else
    result.type = Bound::Type::PlusInfinity;

  result.value = 0;
  return result;
}

IntersectionConstraint
IntersectionConstraint::resolveFutures(const AbstractState &state) const {
  auto resolve = [&](const IntersectionBound &bound,
      bool isLower) -> IntersectionBound {
    if (std::holds_alternative<Bound>(bound))
      return bound;

    const Future &future = std::get<Future>(bound);

    auto it = state.find(future.target_variable);
    assert(it != state.end());

    Bound result =
      isLower ? it->second.getLower()
      : it->second.getUpper();

    if (result.type == Bound::Type::Constant)
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

  // AnalyzedValue result;

  // Source is a finite set.
  if (src.getKind() == AnalyzedValue::Kind::Set) {

    std::vector<int> vals;
    for (int v : src.getValues()) {
      bool keep = true;
      if (low.type == Bound::Type::Constant)
        keep &= (v >= low.value);
      if (up.type == Bound::Type::Constant)
        keep &= (v <= up.value);
      if (keep)
        vals.emplace_back(v);
    }

    if (!vals.empty())
      A[def].addConstant(vals);
  }

  // Source is already an interval.
  else {
    auto lower = src.getLower();
    auto upper = src.getUpper();

    // max(lower, low)
    if (low.type == Bound::Type::Constant) {
      if (lower.type == Bound::Type::MinusInfinity)
        lower = low;
      else if (lower.type == Bound::Type::Constant)
        lower.value = std::max(lower.value, low.value);
    }

    // min(upper, up)
    if (up.type == Bound::Type::Constant) {
      if (upper.type == Bound::Type::PlusInfinity)
        upper = up;
      else if (upper.type == Bound::Type::Constant)
        upper.value = std::min(upper.value, up.value);
    }

    // Empty interval?
    if (lower.type == Bound::Type::Constant &&
        upper.type == Bound::Type::Constant &&
        lower.value > upper.value) {
      // Leave result as bottom.
    } else {
      A[def].setAsInterval(lower, upper, src.getStride());
    }
  }

    return oldValue != A[def];
}

bool MultiplyConstraint::eval(AbstractState &A)
{
  AnalyzedValue old = A[this->def];

  AnalyzedValue lhs = A[this->op1];
  AnalyzedValue rhs = A[this->op2];

  AnalyzedValue result;

  if(lhs.getKind() == AnalyzedValue::Kind::Set && rhs.getKind() == AnalyzedValue::Kind::Set)
  {
    if (lhs.getValues().empty() || rhs.getValues().empty()) {
      A[def] = result;
      return old != result;
    }

    std::vector<int> consts;
    for (int l : lhs.getValues()) {
        for (int r : rhs.getValues()) {
            consts.emplace_back(l * r);
        }
    }
    result.addConstant(consts);
  } else {

    // Otherwise treat every operand as a strided interval.
    auto getLower = [](const AnalyzedValue &v) -> Bound {
      if (v.getKind() == AnalyzedValue::Kind::Set) {
        return Bound::constant(*v.getValues().begin());
      }
      return v.getLower();
    };

    auto getUpper = [](const AnalyzedValue &v) -> Bound {
      if (v.getKind() == AnalyzedValue::Kind::Set) {
        if (v.getValues().empty())
          return Bound::constant(*v.getValues().begin());
                
        return Bound::constant(*v.getValues().rbegin());
      }
      return v.getUpper();
    };

    auto multLower =
        [](const Bound &a,
          const Bound &b) -> Bound {

      if (a.type == Bound::Type::MinusInfinity ||
          b.type == Bound::Type::MinusInfinity)
        return Bound::minusInfinity();

      return Bound::constant(a.getConstant() + b.getConstant());
    };

    auto multUpper =
        [](const Bound &a,
          const Bound &b) -> Bound {

      if (a.type == Bound::Type::PlusInfinity ||
          b.type == Bound::Type::PlusInfinity)
        return Bound::plusInfinity();

      return Bound::constant(a.getConstant() + b.getConstant());
    };

    Bound l1 = getLower(lhs);
    Bound l2 = getLower(rhs);
    Bound u1 = getUpper(lhs);
    Bound u2 = getUpper(rhs);

    // Possible multiplications

    // [0,0] * [x2,y2] -> [0,0]
    // [0,0] * [-inf, +inf] -> [0,0]
    // [-inf, +inf] * [x2,y2] -> [-inf, +inf]

    // [-inf, y1] * [x2,y2] -> [-inf, y1*y2]
    // [-inf, y1] * [0,y2] -> [-inf, y1*y2]
    // [-inf, y1] * [-x2,y2] -> [-inf, +inf]
    // [-inf, y1] * [-x2,0] -> [y1*-x2, +inf]
    // [-inf, y1] * [-x2,-y2] -> [y1*-x2, +inf]

    // [-inf, -y1] * [x2,y2] -> 
    // [-inf, -y1] * [0,y2] -> 
    // [-inf, -y1] * [-x2,y2] -> [-inf, +inf]
    // [-inf, -y1] * [-x2,0] -> 
    // [-inf, -y1] * [-x2,-y2] -> 

    // [x1, +inf] * [x2,y2] -> 
    // [x1, +inf] * [0,y2] -> 
    // [x1, +inf] * [-x2,y2] -> [-inf, +inf]
    // [x1, +inf] * [-x2,0] -> 
    // [x1, +inf] * [-x2,-y2] -> 

    // [-x1, +inf] * [x2,y2] -> 
    // [-x1, +inf] * [0,y2] -> 
    // [-x1, +inf] * [-x2,y2] -> [-inf, +inf]
    // [-x1, +inf] * [-x2,0] -> 
    // [-x1, +inf] * [-x2,-y2] -> 


    int p1 = l1.value * l2.value;
    int p2 = l1.value * u2.value;
    int p3 = u1.value * l2.value;
    int p4 = u1.value * u2.value;

    int min = std::min({p1, p2, p3, p4});
    int max = std::max({p1, p2, p3, p4});

    result.setAsInterval(Bound::constant(min),
                         Bound::constant(max), 1); // FIXME: Interval stride
  }

  A[this->def] = result;
  return old != result;
}

bool LinearConstraint::eval(AbstractState &A)
{
  AnalyzedValue old = A[this->def];
  AnalyzedValue src = A[this->operand];

  AnalyzedValue result;

  if(src.getKind() == AnalyzedValue::Kind::Set)
  {
    std::vector<int> consts;
    for (int v : src.getValues()) {
      consts.emplace_back((a * v) + b);
    }
    result.addConstant(consts);
  }else{
    int k1 = (this->a*src.getLower().value + this->b);
    int ku = (this->a*src.getUpper().value + this->b);

    int min = std::min(k1,ku);
    int max = std::max(k1,ku);

    result.setAsInterval(Bound::constant(min), Bound::constant(max), src.getStride());
  }

  A[this->def] = result;
  return old != result;
}
