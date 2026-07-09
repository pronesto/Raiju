/**
 * @file AbstractValue.h
 * @brief Defines the AbstractValue class implementing the Finite-Set / Strided-Interval abstract domain.
 */

#pragma once

#include <vector>
#include <numeric>
#include <algorithm>

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
     * @brief Maximum number of distinct constants tracked exactly by this domain.
     */
    static constexpr unsigned MaxConstants = N;

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
    Kind kind;                   /**< A Set or a StridedInterval. */
    std::vector<int> values;     /**< Constants when kind == Kind::Set. Unused otherwise. */
    Bound lower;                 /**< Lower bound used when kind == Kind::StridedInterval. */
    Bound upper;                 /**< Upper bound used when kind == Kind::StridedInterval. */
    unsigned stride;             /**< Stride value ($s \ge 1$) for strided intervals. */

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
     * @brief Computes the least upper bound (join) of this value and another value.
     * * If both are sets and the union's size $\le N$, the result is a finite set. 
     * Otherwise, it collapses into or updates a strided interval.
     * * @param other The abstract value to join with.
     */
    void join(const AbstractValue& other);

    AbstractValue<N> intersect(AbstractValue& av);

    /**
     * @brief Adds a single literal constant into the abstract value representation.
     * * @param val The integer constant to add.
     */
    void addConstant(int val);
    
    /**
     * @brief Get the current structural representation kind.
     * @return Kind The structural layout (Set or StridedInterval).
     */
    Kind getKind() const { return kind; }

        /**
     * @brief Returns the set of tracked constants.
     *
     * This method is only meaningful when getKind() == Kind::Set.
     *
     * @return A constant reference to the internal vector of values.
     */
    const std::vector<int>& getValues() const {
        return values;
    }

    /**
     * @brief Returns the lower bound of a strided interval.
     *
     * This method is only meaningful when getKind() ==
     * Kind::StridedInterval.
     *
     * @return A constant reference to the lower bound.
     */
    const Bound& getLower() const {
        return lower;
    }

    /**
     * @brief Returns the upper bound of a strided interval.
     *
     * This method is only meaningful when getKind() ==
     * Kind::StridedInterval.
     *
     * @return A constant reference to the upper bound.
     */
    const Bound& getUpper() const {
        return upper;
    }

    /**
     * @brief Returns the stride of a strided interval.
     *
     * This method is only meaningful when getKind() ==
     * Kind::StridedInterval.
     *
     * @return The interval stride.
     */
    unsigned getStride() const {
        return stride;
    }

    /**
     * @brief Converts this abstract value into a strided interval.
     *
     * The current set representation, if any, is discarded.
     *
     * @param lowerBound Lower bound of the interval.
     * @param upperBound Upper bound of the interval.
     * @param intervalStride Stride of the interval (must be at least one).
     */
    void setAsInterval(const Bound& lowerBound,
                       const Bound& upperBound,
                       unsigned intervalStride = 1) {
        kind = Kind::StridedInterval;
        lower = lowerBound;
        upper = upperBound;
        stride = std::max(1u, intervalStride);
        values.clear();
    }

    /**
     * @brief Equality operator for two Bounds.
     */
    friend bool operator==(const Bound& lhs, const Bound& rhs) {
        if (lhs.type != rhs.type) return false;
        if (lhs.type == Bound::Type::Constant) {
            return lhs.value == rhs.value;
        }
        return true; // Both are either PlusInfinity or MinusInfinity
    }

    /**
     * @brief Inequality operator for two Bounds.
     */
    friend bool operator!=(const Bound& lhs, const Bound& rhs) {
        return !(lhs == rhs);
    }

    /**
     * @brief Less-than operator for two Bounds.
     */
    friend bool operator<(const Bound& lhs, const Bound& rhs) {
      if (lhs.type == rhs.type) {
        if (lhs.type == Bound::Type::Constant) {
          return lhs.value < rhs.value;
        }
        return false; // Both are -Infinity or both are +Infinity
      }
      // Handle distinct types
      if (lhs.type == Bound::Type::MinusInfinity) return true;
      if (lhs.type == Bound::Type::PlusInfinity) return false;
      // lhs is Constant
      return rhs.type == Bound::Type::PlusInfinity;
    }

    friend bool operator<=(const Bound& lhs, const Bound& rhs) {
      return (lhs < rhs) || (lhs == rhs);
    }

    friend bool operator>(const Bound& lhs, const Bound& rhs) {
      return rhs < lhs;
    }

    friend bool operator>=(const Bound& lhs, const Bound& rhs) {
      return !(lhs < rhs);
    }

    /**
     * @brief Checks if two AbstractValues are completely identical in the lattice.
     * @param other The abstract value to compare against.
     * @return true if both states share the exact same abstract representation, false otherwise.
     */
    bool operator==(const AbstractValue& other) const {
        // 1. Structural kind must match
        if (this->kind != other.kind) {
            return false;
        }

        // 2. Element-wise comparison depending on the internal layout
        if (this->kind == Kind::Set) {
            // Since elements are kept sorted and unique, a standard vector
            // comparison works in O(k) time where k <= N.
            return this->values == other.values;
        } else {
            // Strided Interval parameters must be exactly identical
            return (this->stride == other.stride) &&
                   (this->lower == other.lower) &&
                   (this->upper == other.upper);
        }
    }

    /**
     * @brief Inequality operator for AbstractValue.
     */
    bool operator!=(const AbstractValue& other) const {
        return !(*this == other);
    }

    /**
     * @brief Checks if every possible value in 'this' is strictly less than every value in 'other'.
     */
    bool operator<(const AbstractValue& other) const {
      // If either abstract state is empty (bottom), comparison is trivially false
      if ((this->kind == Kind::Set && this->values.empty()) ||
          (other.kind == Kind::Set && other.values.empty())) {
        return false;
      }

      // Extract boundaries for 'this'
      Bound this_upper;
      if (this->kind == Kind::Set) {
        this_upper = Bound{Bound::Type::Constant, this->values.back()};
      } else {
        this_upper = this->upper;
      }

      // Extract boundaries for 'other'
      Bound other_lower;
      if (other.kind == Kind::Set) {
        other_lower = Bound{Bound::Type::Constant, other.values.front()};
      } else {
        other_lower = other.lower;
      }

      // 'this' < 'other' holds true globally if max(this) < min(other)
      return this_upper < other_lower;
    }

    /**
     * @brief Checks if every possible value in 'this' is less than or equal to every
     * value in 'other'.
     */
    bool operator<=(const AbstractValue& other) const {
      if ((this->kind == Kind::Set && this->values.empty()) ||
          (other.kind == Kind::Set && other.values.empty())) {
        return false;
      }

      Bound this_upper = (this->kind == Kind::Set)
        ? Bound{Bound::Type::Constant, this->values.back()} : this->upper;

      Bound other_lower = (other.kind == Kind::Set)
        ? Bound{Bound::Type::Constant, other.values.front()} : other.lower;

      return this_upper <= other_lower;
    }

    /**
     * @brief Checks if every possible value in 'this' is strictly greater
     * than every value in 'other'.
     */
    bool operator>(const AbstractValue& other) const {
      return other < *this;
    }

    /**
     * @brief Checks if every possible value in 'this' is greater than or
     * equal to every value in 'other'.
     */
    bool operator>=(const AbstractValue& other) const {
      return other <= *this;
    }
};

template <unsigned N>
void AbstractValue<N>::addConstant(int val) {
    if (kind == Kind::Set) {
        // Find the insertion position to maintain sorted order
        auto it = std::lower_bound(values.begin(), values.end(), val);

        // If the constant is already present, do nothing (exact match)
        if (it != values.end() && *it == val) {
            return;
        }

        // Insert the new unique constant
        values.insert(it, val);

        // Check if we have exceeded the exact tracking capacity N
        if (values.size() > N) {
            // Collapse the representation into a Strided Interval
            kind = Kind::StridedInterval;

            int base = values.front();
            int current_gcd = 0;
            for (size_t i = 1; i < values.size(); ++i) {
                current_gcd = std::gcd(current_gcd, values[i] - base);
            }

            // Assign bounds based on the captured set bounds
            lower.type = Bound::Type::Constant;
            lower.value = values.front();

            upper.type = Bound::Type::Constant;
            upper.value = values.back();

            stride = (current_gcd == 0) ? 1 : static_cast<unsigned>(current_gcd);

            // Free the memory since vector is no longer used
            values.clear();
            values.shrink_to_fit();
        }
    } else {
        // If it's already a Strided Interval, we apply the widening logic
        // to adapt the bounds and recalculate the stride based on the new point.
        if (val < lower.value && lower.type == Bound::Type::Constant) {
            // Case 2: Constant is smaller than the minimum
            lower.type = Bound::Type::MinusInfinity;
            stride = std::gcd(stride, static_cast<unsigned>(std::abs(upper.value - val)));
        } else if (val > upper.value && upper.type == Bound::Type::Constant) {
            // Case 3: Constant is larger than the maximum
            upper.type = Bound::Type::PlusInfinity;
            stride = std::gcd(stride, static_cast<unsigned>(std::abs(val - lower.value)));
        } else {
            // Case 1: Inside the current hull bounds
            stride = std::gcd(stride, static_cast<unsigned>(std::abs(val - lower.value)));
        }
    }
}

template <unsigned N>
void AbstractValue<N>::join(const AbstractValue& other) {
    // 1. Both are Sets
    if (this->kind == Kind::Set && other.kind == Kind::Set) {
        std::vector<int> merged;
        merged.reserve(this->values.size() + other.values.size());
        
        // Take the sorted union of both sets
        std::set_union(this->values.begin(), this->values.end(),
                       other.values.begin(), other.values.end(),
                       std::back_inserter(merged));
        
        if (merged.size() <= N) {
            this->values = std::move(merged);
        } else {
            // Collapsing into a Strided Interval because we exceeded N
            this->kind = Kind::StridedInterval;
            
            int base = merged.front();
            int current_gcd = 0;
            for (size_t i = 1; i < merged.size(); ++i) {
                current_gcd = std::gcd(current_gcd, merged[i] - base);
            }
            
            this->lower.type = Bound::Type::Constant;
            this->lower.value = merged.front();
            
            this->upper.type = Bound::Type::Constant;
            this->upper.value = merged.back();
            
            this->stride = (current_gcd == 0) ? 1 : static_cast<unsigned>(current_gcd);
            this->values.clear();
        }
        return;
    }

    // 2. Handling mixed or dual Strided Interval states
    // Force 'this' to adapt to an interval configuration if it's currently a set
    if (this->kind == Kind::Set) {
        if (this->values.empty()) {
            // If 'this' is empty (bottom element), simply adopt the other state
            *this = other;
            return;
        }
        // Convert 'this' to an interval before resolving bounds with the other interval
        int base = this->values.front();
        int current_gcd = 0;
        for (size_t i = 1; i < this->values.size(); ++i) {
            current_gcd = std::gcd(current_gcd, this->values[i] - base);
        }
        this->lower.type = Bound::Type::Constant;
        this->lower.value = this->values.front();
        this->upper.type = Bound::Type::Constant;
        this->upper.value = this->values.back();
        this->stride = (current_gcd == 0) ? 1 : static_cast<unsigned>(current_gcd);
        this->kind = Kind::StridedInterval;
        this->values.clear();
    }

    // Now 'this' is definitely a StridedInterval. We process the elements of 'other'.
    if (other.kind == Kind::Set) {
        for (int val : other.values) {
            this->addConstant(val);
        }
    } else {
        // Both are Strided Intervals: Merge the interval boundaries
        
        // Compute Lower Bound
        if (other.lower.type == Bound::Type::MinusInfinity) {
            this->lower.type = Bound::Type::MinusInfinity;
        } else if (this->lower.type == Bound::Type::Constant) {
            this->lower.value = std::min(this->lower.value, other.lower.value);
        }

        // Compute Upper Bound
        if (other.upper.type == Bound::Type::PlusInfinity) {
            this->upper.type = Bound::Type::PlusInfinity;
        } else if (this->upper.type == Bound::Type::Constant) {
            this->upper.value = std::max(this->upper.value, other.upper.value);
        }

        // The stride must decrease to capture the strides of both intervals,
        // as well as the alignment offset between their starting configurations.
        if (this->lower.type == Bound::Type::Constant && other.lower.type == Bound::Type::Constant) {
            int offset = std::abs(this->lower.value - other.lower.value);
            this->stride = std::gcd(std::gcd(this->stride, other.stride), static_cast<unsigned>(offset));
        } else {
            this->stride = std::gcd(this->stride, other.stride);
        }
    }
}


template <unsigned N>
AbstractValue<N> AbstractValue<N>::intersect(AbstractValue& av)
{
    auto low   = std::max(this->getLower(), av.getLower());
    auto upper = std::max(this->getUpper(), av.getUpper());

    AbstractValue<N> res;

    if (low.value > upper.value && low.type == Bound::Constant && upper.type == Bound::Constant) {
        return res; 
    }
    res.setAsInterval(low, upper);
    return res;
}


// Let's work with at most four constants per abstract state. If later on we
// want more precision, then change this definition and recompile.
using AnalyzedValue = AbstractValue<4>;
