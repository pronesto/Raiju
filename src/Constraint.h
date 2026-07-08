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
    const std::string def;

    explicit Constraint(std::string name);
    virtual ~Constraint() = default;

    /**
     * @brief Evaluates the constraint against the current abstract state.
     * @param A The global variable-to-value map.
     * @return true if the variable_name's value changed, false otherwise.
     */
    virtual bool eval(AbstractState& A) = 0;
    
    virtual std::vector<std::string> get_uses() const = 0; 
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

    std::vector<std::string> get_uses() const override {
        std::vector<std::string> ret = {};
        return ret;
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

    std::vector<std::string> get_uses() const override {
      return operands;
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

   std::vector<std::string> get_uses(){
      return {op1, op2};
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

    // An intersection boundary can be a literal Constant, an Infinity, or a Future
    using IntersectionBound = std::variant<AnalyzedValue::Bound, Future>;

private:
    std::string operand;
    IntersectionBound lower_bound;
    IntersectionBound upper_bound;

    // Helper to resolve a variant bound into a concrete AnalyzedValue::Bound at runtime
    AnalyzedValue::Bound resolveBound(const IntersectionBound& b, const bool isLower,
        const AbstractState& A) const;

public:
    IntersectionConstraint(std::string dest, std::string src,
                           IntersectionBound low, IntersectionBound up);
    bool eval(AbstractState& A) override;


    std::vector<std::string> get_uses() const override {

      std::vector<std::string> uses;
      uses.push_back(operand);

      if(std::holds_alternative<Future>(lower_bound)){
         uses.push_back(std::get<Future>(lower_bound).target_variable);
      }

      if(std::holds_alternative<Future>(upper_bound)){
         uses.push_back(std::get<Future>(upper_bound).target_variable);
      }

      return uses;
    }
};


