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

    if (a.isMinusInfinity() ||
        b.isMinusInfinity())
      return Bound::minusInfinity();

    return Bound::constant(a.getConstant() + b.getConstant());
  };

  auto addUpper =
      [](const Bound &a,
         const Bound &b) -> Bound {

    if (a.isPlusInfinity() ||
        b.isPlusInfinity())
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
    result = Bound::minusInfinity();
  else
    result = Bound::plusInfinity();

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

    if (result.isConstant())
      result.setConstant(result.getConstant() + future.offset);

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
      if (low.isConstant())
        keep &= (v >= low.getConstant());
      if (up.isConstant())
        keep &= (v <= up.getConstant());
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
    if (low.isConstant()) {
      if (lower.isMinusInfinity())
        lower = low;
      else if (lower.isConstant())
        lower.setConstant(std::max(lower.getConstant(), low.getConstant()));
    }

    // min(upper, up)
    if (up.isConstant()) {
      if (upper.isPlusInfinity())
        upper = up;
      else if (upper.isConstant())
        upper.setConstant(std::min(upper.getConstant(), up.getConstant()));
    }

    // Empty interval?
    if (lower.isConstant() &&
        upper.isConstant() &&
        lower.getConstant() > upper.getConstant()) {
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

    Bound l1 = getLower(lhs);
    Bound l2 = getLower(rhs);
    Bound u1 = getUpper(lhs);
    Bound u2 = getUpper(rhs);

    auto hasInfiniteBound = [](const AnalyzedValue& v) -> bool {
      return (!v.getLower().isConstant() || !v.getUpper().isConstant());
    };

    auto solveInfinite = [](const Bound &x1, const Bound &y1, const Bound &x2, const Bound &y2) -> std::pair<Bound, Bound> {
      // 2. Case: [-inf, +inf] * [anything other than zero] is [-inf, +inf]
      if (x1.isMinusInfinity() && y1.isPlusInfinity()) {
        if (x2.isConstant() && x2.getConstant() == 0 && y2.isConstant() && y2.getConstant() == 0)
          return std::make_pair(Bound::constant(0), Bound::constant(0));
        return std::make_pair(Bound::minusInfinity(), Bound::plusInfinity());
      }

      // 3. Normalized flags for readability
      bool x1_inf = x1.isMinusInfinity();
      bool y1_inf = y1.isPlusInfinity();
      bool x2_pos = x2.isConstant() && x2.getConstant() >= 0;
      bool y2_neg = y2.isConstant() && y2.getConstant() <= 0;

      auto inf = []() -> std::pair<Bound, Bound> { 
        return std::make_pair(Bound::minusInfinity(), Bound::plusInfinity()); 
      };

      auto minusInfBound = [](const int product) -> std::pair<Bound, Bound> { 
        return std::make_pair(Bound::minusInfinity(), Bound::constant(product)); 
      };
      
      auto plusInfBound= [](const int product) -> std::pair<Bound, Bound> {
        return std::make_pair(Bound::constant(product), Bound::plusInfinity()); 
      };

      // Logic based on which side the infinity is on and the sign of the multiplier [x2, y2]
      if (x1_inf) {
          if (y1.getConstant() > 0) { // [-inf, +]
              if (x2_pos) return minusInfBound(y1.getConstant() * y2.getConstant());
              if (y2_neg) return plusInfBound(y1.getConstant() * x2.getConstant());
              return inf();
          } else { // [-inf, -]
              if (x2_pos) return minusInfBound(y1.getConstant() * x2.getConstant());
              if (y2_neg) return plusInfBound(y1.getConstant() * y2.getConstant());
              return inf();
          }
      } 
      
      if (y1_inf) {
          if (x1.getConstant() > 0) { // [+, +inf]
              if (x2_pos) return plusInfBound(x1.getConstant() * x2.getConstant());
              if (y2_neg) return minusInfBound(x1.getConstant() * y2.getConstant());
              return inf();
          } else { // [-, +inf]
              if (x2_pos) return plusInfBound(x1.getConstant() * y2.getConstant());
              if (y2_neg) return minusInfBound(x1.getConstant() * x2.getConstant());
              return inf();
          }
      }
    };

    if (hasInfiniteBound(lhs)) {
      auto [newLow, newUp] = solveInfinite(l1, u1, l2, u2);
      result.setAsInterval(newLow, newUp, 1);
    } else if (hasInfiniteBound(rhs)) {
      auto [newLow, newUp] = solveInfinite(l2, u2, l1, u1);
      result.setAsInterval(newLow, newUp, 1);
    } else {
      int p1 = l1.getConstant() * l2.getConstant();
      int p2 = l1.getConstant() * u2.getConstant();
      int p3 = u1.getConstant() * l2.getConstant();
      int p4 = u1.getConstant() * u2.getConstant();

      int min = std::min({p1, p2, p3, p4});
      int max = std::max({p1, p2, p3, p4});

      result.setAsInterval(Bound::constant(min),Bound::constant(max), 1); // FIXME: Interval stride
    }
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
    Bound srcLower = src.getLower();
    Bound srcUpper = src.getUpper();
    
    Bound lower, upper;
    
    if (this->a == 0)
        result.setAsInterval(Bound::constant(b), Bound::constant(b), src.getStride());
    else if (srcLower.isConstant() && srcUpper.isConstant()) {
      int k1 = (this->a * srcLower.getConstant() + this->b);
      int ku = (this->a * srcUpper.getConstant() + this->b);
      int min = std::min(k1,ku);
      int max = std::max(k1,ku);
      result.setAsInterval(Bound::constant(min), Bound::constant(max), src.getStride());
    } else {
      if (srcLower.isMinusInfinity() && srcUpper.isPlusInfinity()) {
        result.setAsInterval(Bound::minusInfinity(), Bound::plusInfinity(), 1);
      } else if (srcLower.isMinusInfinity()) {
        int k = (this->a * srcUpper.getConstant() + this->b);
        if (this->a > 0)
          result.setAsInterval(Bound::minusInfinity(), Bound::constant(k), 1);
        else
          result.setAsInterval(Bound::constant(k), Bound::plusInfinity(), 1);
      } else {
        int k = (this->a * srcLower.getConstant() + this->b);
        if (this->a > 0)
          result.setAsInterval(Bound::constant(k), Bound::plusInfinity(), 1);
        else
          result.setAsInterval(Bound::minusInfinity(), Bound::constant(k), 1);
      }
    }
  }

  A[this->def] = result;
  return old != result;
}
