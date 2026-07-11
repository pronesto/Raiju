/**
 * @file Constraint.h
 * @brief Declarations for the SSA Constraint hierarchy.
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include "AbstractValue.h"

// Define our global abstract state table using the alias
using AbstractState = std::unordered_map<std::string, AnalyzedValue>;

/**
 * @class Constraint
 * @brief Abstract base class for all dataflow equations.
 */
class Constraint {
public:
    const std::string variable_name;

    explicit Constraint(std::string name);
    virtual ~Constraint() = default;

    /**
     * @brief Evaluates the constraint against the current abstract state.
     * @param A The global variable-to-value map.
     * @return true if the variable_name's value changed, false otherwise.
     */
    virtual bool eval(AbstractState& A) = 0;

    /**
     * @brief Refines the abstract value of a variable using a monotonic
     * narrowing operator.
     * @param A The current abstract state map tracking variable domain
     * evaluations.
     * @return true if the abstract value was successfully narrowed (shrunk),
     * false if the domain remained unchanged (indicating a fixed point).
     */
    bool narrow(AbstractState& A) {
      using Bound = AnalyzedValue::Bound;
      using Type  = Bound::Type;

      AnalyzedValue oldY = A[variable_name];   // I[Y]

      // force eval()'s bottom-case branch
      A[variable_name] = AnalyzedValue();      
      eval(A);                                 // e(Y)
      AnalyzedValue eY = A[variable_name];

      Bound lo = oldY.getLower();
      Bound hi = oldY.getUpper();

      // 1. Guard 1: I[Y] is -Infinity, and e(Y) has recovered to a finite bound
      if (oldY.getLower().type == Type::MinusInfinity &&
          eY.getLower().type  != Type::MinusInfinity) {
        lo = eY.getLower();
      }
      // 3. Guard 3: e(Y) lower bound is greater (tighter) than oldY lower
      // bound -> Narrow!
      else if (eY.getLower() > oldY.getLower()) {
        lo = eY.getLower();
      }

      // 2. Guard 2: I[Y] is +Infinity, and e(Y) has recovered to a finite bound
      if (oldY.getUpper().type == Type::PlusInfinity &&
          eY.getUpper().type  != Type::PlusInfinity) {
        hi = eY.getUpper();
      }
      // 4. Guard 4: e(Y) upper bound is smaller (tighter) than oldY upper
      // bound -> Narrow!
      else if (eY.getUpper() < oldY.getUpper()) {
        hi = eY.getUpper();
      }

      AnalyzedValue result;
      result.setAsInterval(lo, hi, 1);
      A[variable_name] = result;

      // Termination relies on this returning false when no further shrinking
      // occurs
      return result != oldY;

    }
};

/**
 * @class InitializationConstraint
 * @brief Models literal assignments: v = c
 */
class InitializationConstraint : public Constraint {
private:
    int constant;
public:
    InitializationConstraint(std::string var, int c);
    bool eval(AbstractState& A) override;
};

/**
 * @class PhiConstraint
 * @brief Models SSA control-flow merges: v0 = phi(v1, v2, ..., vk)
 */
class PhiConstraint : public Constraint {
private:
    std::vector<std::string> operands;
public:
    PhiConstraint(std::string var, std::vector<std::string> ops);
    bool eval(AbstractState& A) override;
};

/**
 * @class ArithmeticConstraint
 * @brief Base class for binary arithmetic constraints tracking two operands.
 */
class ArithmeticConstraint : public Constraint {
protected:
    std::string op1;
    std::string op2;
public:
    ArithmeticConstraint(std::string dest, std::string lhs, std::string rhs);
};

/**
 * @class AddConstraint
 * @brief Models abstract addition: v0 = v1 + v2
 */
class AddConstraint : public ArithmeticConstraint {
public:
    using ArithmeticConstraint::ArithmeticConstraint;
    bool eval(AbstractState& A) override;
};


/**
 * @class IntersectionConstraint
 * @brief Models narrowing blocks: v0 = v1 intersection [low, up]
 * The bound endpoints can either be a static integer constant, an infinity,
 * or a "Future" referencing another variable.
 */
class IntersectionConstraint : public Constraint {
public:
    // A Future represents a symbolic reference to another variable's state
    struct Future {
        std::string target_variable;
        int offset; // Handles relations like Future(y) - 1 or Future(x) + 1
    };

    // An intersection boundary can be a literal Constant, an Infinity, or a
    // Future
    using IntersectionBound = std::variant<AnalyzedValue::Bound, Future>;

    // @brief Replace symbolic bounds with concrete bounds.
    // @param state The table with abstract states that we will inspect to
    //   resolve symbolic bounds.
    IntersectionConstraint resolveFutures(
      const AbstractState &state) const;

private:
    std::string operand;
    IntersectionBound lower_bound;
    IntersectionBound upper_bound;

    // Helper to resolve a variant bound into a concrete AnalyzedValue::Bound
    // at runtime
    AnalyzedValue::Bound resolveBound(
        const IntersectionBound& b,
        const bool isLower,
        const AbstractState& A
        ) const;

public:
    IntersectionConstraint(std::string dest, std::string src,
                           IntersectionBound low, IntersectionBound up);
    bool eval(AbstractState& A) override;
};

/**
 * @class MultiplyConstraint
 * @brief Models abstract addition: v0 = v1 * v2
 */
class MultiplyConstraint : public ArithmeticConstraint {
public:
    using ArithmeticConstraint::ArithmeticConstraint;
    bool eval(AbstractState& A) override;
};

class LinearConstraint : public Constraint{
    std::string operand;
    int a, b;

    public:
        LinearConstraint(std::string dest, std::string src, int multiplier, int offset)
        : Constraint(std::move(dest)), operand(std::move(src)), a(multiplier), b(offset) {}

        bool eval(AbstractState& A) override;
};