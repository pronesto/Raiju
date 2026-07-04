/**
 * @file AbstractValue.h
 * @brief Defines the AbstractValue class implementing the Finite-Set / Strided-Interval abstract domain.
 */

#pragma once

#include <vector>

/**
 * @class AbstractValue
 * @brief Represents an element in the Finite-Set / Strided-Interval abstract domain.
 * * This class tracks numerical variables during static analysis. It maintains an exact 
 * set of constants up to a capacity @p N. If the number of constants exceeds @p N, 
 * it collapses into a strided interval representation to ensure a bounded abstract state.
 * * @tparam N The compile-time constant determining the maximum size of exact sets.
 */
template <unsigned N>
class AbstractValue {
public:
    /**
     * @enum Kind
     * @brief Represents the current underlying representation of the abstract value.
     */
    enum class Kind {
        Set,             /**< The value is tracked exactly as a finite set of constants. */
        StridedInterval  /**< The value has collapsed into a strided interval. */
    };

    /**
     * @struct Bound
     * @brief Represents a boundary point of a strided interval, handling infinities.
     */
    struct Bound {
        /**
         * @enum Type
         * @brief Defines the nature of the interval bound.
         */
        enum class Type {
            MinusInfinity, /**< Represents negative infinity (-\infty). */
            Constant,      /**< Represents a precise integer constant. */
            PlusInfinity   /**< Represents positive infinity (+\infty). */
        };

        Type type;  /**< The type of the bound. */
        int value;  /**< The literal value if type is Type::Constant. */
    };

private:
    Kind kind;                   /**< Keeps track of whether this is a Set or a StridedInterval. */
    std::vector<int> values;     /**< Stores exact constants when kind == Kind::Set. Unused otherwise. */
    Bound lower;                 /**< Lower bound used when kind == Kind::StridedInterval. */
    Bound upper;                 /**< Upper bound used when kind == Kind::StridedInterval. */
    unsigned stride;             /**< Stride value ($s \ge 1$) used when kind == Kind::StridedInterval. */

public:
    /**
     * @brief Default constructor initializing to an empty set (the bottom element).
     */
    AbstractValue() : kind(Kind::Set), stride(1) {
        lower.type = Bound::Type::Constant;
        lower.value = 0;
        upper.type = Bound::Type::Constant;
        upper.value = 0;
    }

    /**
     * @brief Computes the least upper bound (join) of this value and another abstract value.
     * * If both are sets and the union's size $\le N$, the result is a finite set. 
     * Otherwise, it collapses into or updates a strided interval.
     * * @param other The abstract value to join with.
     */
    void join(const AbstractValue& other);

    /**
     * @brief Forcefully adds a single literal constant into the abstract value representation.
     * * @param val The integer constant to add.
     */
    void addConstant(int val);
    
    /**
     * @brief Get the current structural representation kind.
     * @return Kind The structural layout (Set or StridedInterval).
     */
    Kind getKind() const { return kind; }
};
