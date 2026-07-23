/**
 * @file Constraint.h
 * @brief Declarations for the SSA Constraint hierarchy.
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <variant>
#include "IntValue.h"

enum class EdgeType{
   Data,
   Future,
};

struct UseEdge{
   std::string target_variable;
   EdgeType type;
};

// Define our global abstract state table using the alias
using AbstractState = std::unordered_map<std::string, AnalyzedValue>;

/**
 * @class Constraint
 * @brief Abstract base class for all dataflow equations.
 */
class Constraint {
public:
    const std::string def;

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
      using Type  = Bound::Type;

      AnalyzedValue oldY = A[def];   // I[Y]

      // force eval()'s bottom-case branch
      A[def].setAsBottom();     
      eval(A);                                 // e(Y)
      AnalyzedValue eY = A[def];

      if (oldY.getKind() == AnalyzedValue::Kind::Set && eY.getKind() == AnalyzedValue::Kind::Set) {
        AnalyzedValue result = oldY;
        std::vector<int> vals;
        for (auto val : eY.getValues()) {
          vals.emplace_back(val);
        }
        result.addConstant(vals);
        A[def] = result;
        return result != oldY;
      }

      Bound lo = oldY.getLower();
      Bound hi = oldY.getUpper();

      // 1. Guard 1: I[Y] is -Infinity, and e(Y) has recovered to a finite bound
      if (oldY.getLower().isMinusInfinity() &&
          !eY.getLower().isMinusInfinity()) {
        lo = eY.getLower();
      }
      // 3. Guard 3: e(Y) lower bound is greater (tighter) than oldY lower
      // bound -> Narrow!
      else if (eY.getLower() > oldY.getLower()) {
        lo = eY.getLower();
      }

      // 2. Guard 2: I[Y] is +Infinity, and e(Y) has recovered to a finite bound
      if (oldY.getUpper().isPlusInfinity() &&
          !eY.getUpper().isPlusInfinity()) {
        hi = eY.getUpper();
      }
      // 4. Guard 4: e(Y) upper bound is smaller (tighter) than oldY upper
      // bound -> Narrow!
      else if (eY.getUpper() < oldY.getUpper()) {
        hi = eY.getUpper();
      }

      A[def].setAsInterval(lo, hi, 1);

      // Termination relies on this returning false when no further shrinking
      // occurs
      return A[def] != oldY;

    }
    
    virtual std::vector<UseEdge> get_uses() const = 0; 
    std::string get_def(){
      return def;
    }

    friend std::ostream& operator<<(std::ostream& os, const Constraint& c) {
        os << c.def;
        return os;
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

    std::vector<UseEdge> get_uses() const override {
        std::vector<UseEdge> ret = {};
        return ret;
    }

    friend std::ostream& operator<<(std::ostream& os, const InitializationConstraint& c) {
        os << c.def << ": " << c.constant;
        return os;
    }
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

    std::vector<UseEdge> get_uses() const override {
      std::vector<UseEdge> edges;
      edges.reserve(operands.size());

      for(const std::string& op : operands) {
         edges.push_back({op, EdgeType::Data});
      }
      
      return edges;
    }

    friend std::ostream& operator<<(std::ostream& os, const PhiConstraint c) {
        os << c.def << ": φ(";
        for (int i = 0; i < c.operands.size(); i++) {
            if (i > 0) os << ", ";
            os << c.operands[i];
        }
        os << ")";
        return os;
    }
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

   std::vector<UseEdge> get_uses() const override{
      return {{op1, EdgeType::Data}, {op2, EdgeType::Data}};
   }
};

/**
 * @class AddConstraint
 * @brief Models abstract addition: v0 = v1 + v2
 */
class AddConstraint : public ArithmeticConstraint {
public:
    using ArithmeticConstraint::ArithmeticConstraint;
    bool eval(AbstractState& A) override;

    friend std::ostream& operator<<(std::ostream& os, const AddConstraint c) {
        os << c.def << ": " << c.op1 << " + " << c.op2;
        return os;
    }
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
    using IntersectionBound = std::variant<Bound, Future>;

    friend std::ostream& operator<<(std::ostream& os, const IntersectionBound b) {
        if (std::holds_alternative<Bound>(b)) {
            os << std::get<Bound>(b);
        } else {
            const Future &f = std::get<Future>(b);
            os << "f(" << f.target_variable << ")";
            if (f.offset > 0) os << " + " << f.offset;
            else if (f.offset < 0) os << " - " << -f.offset;
        }
        return os;
    }

    // @brief Replace symbolic bounds with concrete bounds.
    // @param state The table with abstract states that we will inspect to
    //   resolve symbolic bounds.
    IntersectionConstraint resolveFutures(
      const AbstractState &state) const;

private:
    std::string operand;
    IntersectionBound lower_bound;
    IntersectionBound upper_bound;

    // Helper to resolve a variant bound into a concrete Bound
    // at runtime
    Bound resolveBound(
        const IntersectionBound& b,
        const bool isLower,
        const AbstractState& A
        ) const;

public:
    IntersectionConstraint(std::string dest, std::string src,
                           IntersectionBound low, IntersectionBound up);
    bool eval(AbstractState& A) override;


    std::vector<UseEdge> get_uses() const override {

      std::vector<UseEdge> uses;
      uses.push_back({operand, EdgeType::Data});

      if(std::holds_alternative<Future>(lower_bound)){
         uses.push_back({std::get<Future>(lower_bound).target_variable, EdgeType::Future});
      }

      if(std::holds_alternative<Future>(upper_bound)){
         uses.push_back({std::get<Future>(upper_bound).target_variable, EdgeType::Future});
      }

      return uses;
    }

    friend std::ostream& operator<<(std::ostream& os, const IntersectionConstraint c) {
        os << c.def << ": " << c.operand << " ∩ "
            << "[" << c.lower_bound << "," << c.upper_bound <<  "]";
        return os;
    }
};

/**
 * @class MultiplyConstraint
 * @brief Models abstract multiplication: v0 = v1 * v2
 */
class MultiplyConstraint : public ArithmeticConstraint {
public:
    using ArithmeticConstraint::ArithmeticConstraint;
    bool eval(AbstractState& A) override;

    friend std::ostream& operator<<(std::ostream& os, const MultiplyConstraint c) {
        os << c.def << ": " << c.op1 << " * " << c.op2;
        return os;
    }
};

/**
 * @class LinearConstraint
 * @brief Models abstract linear equations: v0 = a * v1 + b
 */
class LinearConstraint : public Constraint{
private:
    std::string operand;
    int a, b;

public:
    LinearConstraint(std::string dest, std::string src, int multiplier, int offset)
    : Constraint(std::move(dest)), operand(std::move(src)), a(multiplier), b(offset) {}

    bool eval(AbstractState& A) override;

    std::vector<UseEdge> get_uses() const override{
        return {{operand, EdgeType::Data}};
    }

    friend std::ostream& operator<<(std::ostream& os, const LinearConstraint c) {
        os << c.def << ": " << c.a << " * " << c.operand << " + " << c.b;
        return os;
    }
};

inline std::ostream& operator<<(std::ostream& os, const std::shared_ptr<Constraint>& c) {
    if (auto ic = std::dynamic_pointer_cast<InitializationConstraint>(c)) {
        os << *ic;
    } else if (auto pc = std::dynamic_pointer_cast<PhiConstraint>(c)) {
        os << *pc;
    } else if (auto ac = std::dynamic_pointer_cast<AddConstraint>(c)) {
        os << *ac;
    } else if (auto ic = std::dynamic_pointer_cast<IntersectionConstraint>(c)) {
        os << *ic;
    } else if (auto mc = std::dynamic_pointer_cast<MultiplyConstraint>(c)) {
        os << *mc;
    } else if (auto lc = std::dynamic_pointer_cast<LinearConstraint>(c)) {
        os << *lc;
    } else {
        os << *c.get();
    }
    return os;
}

/**
 * @class SubConstraint
 * @brief Models abstract sub: v0 = v1 - v2
 */
class SubConstraint : public ArithmeticConstraint {
public:
    using ArithmeticConstraint::ArithmeticConstraint;
    bool eval(AbstractState& A) override;

    friend std::ostream& operator<<(std::ostream& os, const SubConstraint c) {
        os << c.def << ": " << c.op1 << " + " << c.op2;
        return os;
    }
};