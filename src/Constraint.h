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
    virtual AnalyzedValue evaluateRHS(AbstractState& A) = 0;
    bool narrow(AbstractState& A, AnalyzedValue& e_Y)
    {

        AnalyzedValue i_Y = A[this->variable_name];
        
        AnalyzedValue::Bound::Type  minus_inf = AnalyzedValue::Bound::Type::MinusInfinity;
        AnalyzedValue::Bound::Type  inf = AnalyzedValue::Bound::Type::PlusInfinity; 

        AnalyzedValue::Bound new_upper;
        AnalyzedValue::Bound new_lower;

        if(i_Y.getLower().type == minus_inf && e_Y.getLower().type > minus_inf)
        {
            new_lower = e_Y.getLower();
        }else if(i_Y.getUpper().type == inf && e_Y.getUpper().type < inf)
        {
            new_upper = e_Y.getUpper();
        }else if(i_Y.getLower().type > e_Y.getLower().type)
        {
            new_lower = e_Y.getLower();
        }else if(i_Y.getUpper().type < e_Y.getUpper().type)
        {
            new_upper = e_Y.getUpper();
        }

        if (new_lower.type != i_Y.getLower().type || new_upper.type != i_Y.getUpper().type) {
            AnalyzedValue av;
            av.setAsInterval(new_lower, new_upper);
            A[this->variable_name] = av;
            return true;
        }
        return false;
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
    AnalyzedValue evaluateRHS(AbstractState& A) override;
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
    AnalyzedValue evaluateRHS(AbstractState& A) override;
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
    AnalyzedValue evaluateRHS(AbstractState& A) override;
    bool eval(AbstractState& A) override;
};
