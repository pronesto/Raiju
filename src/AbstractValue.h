/**
 * @file AbstractValue.h
 * @brief Defines the AbstractValue class implementing the Finite-Set /
 * Strided-Interval abstract domain.
 */

#pragma once

#include <set>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <iterator>
#include "Bound.h"

/**
 * @class AbstractValue
 * @brief Represents an element in the Finite-Set / Strided-Interval abstract
 * domain.
 *
 * This class tracks numerical variables during static analysis. It maintains
 * an exact set of constants up to a capacity @p N. If the number of constants
 * exceeds @p N, then it collapses into a strided interval representation to
 * ensure a bounded abstract state.
 *
 * @tparam N The compile-time constant determining the maximum size of exact
 * sets.
 */
template <unsigned N> class AbstractValue {
public:
  /**
   * @brief Maximum number of distinct constants tracked exactly by this
   * domain.
   */
  static constexpr unsigned MaxConstants = N;

  /**
   * @brief Maximum number of operations before applying widening.
   */
  static constexpr unsigned WideningDelay = 4;

  /**
   * @enum Kind
   * @brief Represents the current underlying representation of the abstract
   * value.
   */
  enum class Kind {
    Set,            /**< The value is tracked as a set of constants. */
    StridedInterval /**< The value collapses into a strided interval. */
  };

private:
    Kind kind = Kind::Set;                   /**< A Set or a StridedInterval. */
    std::set<int> values;                    /**< Constants when kind == Kind::Set. Unused otherwise. */
    Bound lower = Bound::constant(0);        /**< Lower bound used when kind == Kind::StridedInterval. */
    Bound upper = Bound::constant(0);        /**< Upper bound used when kind == Kind::StridedInterval. */
    unsigned stride = 1;                     /**< Stride value ($s \ge 1$) for strided intervals. */
    unsigned wideningCounter = 0;            /**< How many operations have run without widening */

public:
  /**
   * @brief Default constructor initializing to an empty set (the bottom
   * element).
   */
  AbstractValue() = default;

  void setAsBottom() {
    kind = Kind::Set;
    values.clear();
    lower = Bound::constant(0);
    upper = Bound::constant(0);
    stride = 1;
    wideningCounter = 0;
  }

  /**
   * @brief Computes the least upper bound (join) of this value and another
   * value.
   * * If both are sets and the union's size $\le N$, the result is a finite
   * set. Otherwise, it collapses into or updates a strided interval.
   * * @param other The abstract value to join with.
   */
  void join(const AbstractValue &other);

    /**
     * @brief Add literal constants into the abstract value representation.
     * * @param vals The integer constants to add.
     */
    void addConstant(const std::vector<int> &vals);

    /**
     * @brief Set counter to 0
     */
    void resetCounter() { wideningCounter = 0; }

    /**
     * @brief Change Kind from Set to StridedInterval
     */
    void changeKind() { kind = Kind::StridedInterval; }

    /**
     * @brief Set a new stride of a StridedInterval
     * * @param val The integer value of the new stride.
     */
    void setStride(unsigned _stride) { stride = _stride; }
    
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
     * @return A constant reference to the internal set of values.
     */
    const std::set<int>& getValues() const {
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
  const Bound &getLower() const { return lower; }

  /**
   * @brief Returns the upper bound of a strided interval.
   *
   * This method is only meaningful when getKind() ==
   * Kind::StridedInterval.
   *
   * @return A constant reference to the upper bound.
   */
  const Bound &getUpper() const { return upper; }

  /**
   * @brief Returns the stride of a strided interval.
   *
   * This method is only meaningful when getKind() ==
   * Kind::StridedInterval.
   *
   * @return The interval stride.
   */
  unsigned getStride() const { return stride; }

  /**
   * @brief Converts this abstract value into a strided interval.
   *
   * The current set representation, if any, is discarded.
   *
   * @param lowerBound Lower bound of the interval.
   * @param upperBound Upper bound of the interval.
   * @param intervalStride Stride of the interval (must be at least one).
   */
  void setAsInterval(const Bound &lowerBound, const Bound &upperBound,
                     unsigned intervalStride = 1) {
    kind = Kind::StridedInterval;

    const Bound oldLower = lower;
    const Bound oldUpper = upper;

    lower = lowerBound;
    upper = upperBound;
    
    bool incrementCounter = false;

    if (lowerBound < oldLower) {
      if (wideningCounter < WideningDelay) incrementCounter = true;
      else lower = Bound::minusInfinity();
    } else if (oldLower.isMinusInfinity()) {
      lower = Bound::minusInfinity();
    }

    if (upperBound > oldUpper) {
      if (wideningCounter < WideningDelay) incrementCounter = true;
      else upper = Bound::plusInfinity();
    } else if (oldUpper.isPlusInfinity()) {
      upper = Bound::plusInfinity();
    }

    if (incrementCounter)
      ++wideningCounter;

    stride = std::max(1u, intervalStride);
    values.clear();
  }

  /**
   * @brief Checks if two AbstractValues are completely identical in the
   * lattice.
   * @param other The abstract value to compare against.
   * @return true if both states share the exact same abstract
   * representation, false otherwise.
   */
  bool operator==(const AbstractValue &other) const {
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
      return (this->stride == other.stride) && (this->lower == other.lower) &&
             (this->upper == other.upper);
    }
  }

  /**
   * @brief Inequality operator for AbstractValue.
   */
  bool operator!=(const AbstractValue &other) const {
    return !(*this == other);
  }

  /**
   * @brief Checks if every possible value in 'this' is strictly less than
   * every value in 'other'.
   */
  bool operator<(const AbstractValue &other) const {
    // If either abstract state is empty (bottom), comparison is trivially
    // false
    if ((this->kind == Kind::Set && this->values.empty()) ||
        (other.kind == Kind::Set && other.values.empty())) {
      return false;
    }

    // Extract boundaries for 'this'
    Bound this_upper;
    if (this->kind == Kind::Set) {
      this_upper = Bound::constant(*this->values.rbegin());
    } else {
      this_upper = this->upper;
    }

    // Extract boundaries for 'other'
    Bound other_lower;
    if (other.kind == Kind::Set) {
      other_lower = Bound::constant(*other.values.begin());
    } else {
      other_lower = other.lower;
    }

    // 'this' < 'other' holds true globally if max(this) < min(other)
    return this_upper < other_lower;
  }

  /**
   * @brief Checks if every possible value in 'this' is less than or equal to
   * every value in 'other'.
   */
  bool operator<=(const AbstractValue &other) const {
    if ((this->kind == Kind::Set && this->values.empty()) ||
        (other.kind == Kind::Set && other.values.empty())) {
      return false;
    }

    Bound this_upper = (this->kind == Kind::Set)
                           ? Bound::constant(*this->values.rbegin())
                           : this->upper;

    Bound other_lower = (other.kind == Kind::Set)
                            ? Bound::constant(*other.values.begin())
                            : other.lower;

    return this_upper <= other_lower;
  }

  /**
   * @brief Checks if every possible value in 'this' is strictly greater
   * than every value in 'other'.
   */
  bool operator>(const AbstractValue &other) const { return other < *this; }

  /**
   * @brief Checks if every possible value in 'this' is greater than or
   * equal to every value in 'other'.
   */
  bool operator>=(const AbstractValue &other) const { return other <= *this; }

  /**
   * @brief Overload for printing the entire AbstractValue state.
   */
  friend std::ostream& operator<<(std::ostream& os, const AbstractValue& av) {
    if (av.kind == Kind::Set) {
      os << "{";
      bool first = true;
      for (int val : av.values) {
        if (!first) os << ", ";
        os << val;
        first = false;
      }
      os << "}";
    } else { // Kind::StridedInterval
      os << "[" << av.lower << ", " << av.upper << "] "
        << "stride: " << av.stride;
    }
    return os;
  }
};

template <unsigned N>
void AbstractValue<N>::addConstant(const std::vector<int> &vals) {
  // If there's no constant to add, we can ignore it
  if (vals.empty()) return;

  bool incrementCounter = false;

  if (kind == Kind::Set) {
    for (int val : vals)
        if (values.emplace(val).second) incrementCounter = true;

    // Assign bounds based on the captured set bounds
    lower = Bound::constant(*values.begin());
    upper = Bound::constant(*values.rbegin());

    // Check if we have exceeded the exact tracking capacity N
    if (values.size() > N) {
      // Collapse the representation into a Strided Interval
      kind = Kind::StridedInterval;

      int base = *values.begin();
      int g = 0;
      for (int v : values)
        if (v != base)
          g = std::gcd(g, v - base);

      stride = (g == 0) ? 1 : static_cast<unsigned>(g);

      values.clear();
    }
  } else {
    for (int val : vals) {
      // If it's already a Strided Interval, we apply the widening logic
      // to adapt the bounds and recalculate the stride based on the new point.
      if (lower.isConstant() && val < lower.getConstant()) {
        // Case 2: Constant is smaller than the minimum
        if (wideningCounter < WideningDelay) {
          if (upper.isConstant()) {
            stride = std::gcd(stride, static_cast<unsigned>(std::abs(val - upper.getConstant())));
          } else {
            stride = 1;
          }
          lower = Bound::constant(val);
          incrementCounter = true;
        } else {
          lower = Bound::minusInfinity();
          stride = 1;
        }
      } else if (upper.isConstant() && val > upper.getConstant()) {
        // Case 3: Constant is larger than the maximum
        if (wideningCounter < WideningDelay) {
          if (lower.isConstant()) {
            stride = std::gcd(stride, static_cast<unsigned>(std::abs(val - lower.getConstant())));
          } else {
            stride = 1;
          }
          upper = Bound::constant(val);
          incrementCounter = true;
        } else {
          upper = Bound::plusInfinity();
          stride = 1;
        }
      } else {
        // Case 1: Inside the current hull bounds
        if (lower.isConstant()) {
          stride = std::gcd(stride, static_cast<unsigned>(std::abs(val - lower.getConstant())));
        } else {
          stride = 1;
        }
      }
    }
  }
  if (incrementCounter)
    ++wideningCounter;
}

template <unsigned N> void AbstractValue<N>::join(const AbstractValue &other) {
  // 1. Both are Sets
  if (this->kind == Kind::Set && other.kind == Kind::Set) {
    std::set<int> merged;

    // Take the sorted union of both sets
    std::set_union(this->values.begin(), this->values.end(),
                   other.values.begin(), other.values.end(),
                   std::inserter(merged, merged.begin()));

    bool incrementCounter = false;
    if (merged.size() <= N) {
      this->values = std::move(merged);
    } else {
      this->kind = Kind::StridedInterval;

      int oldMin = *this->values.begin();
      int oldMax = *this->values.rbegin();

      int newMin = *merged.begin();
      int newMax = *merged.rbegin();

      // Lower bound widens only if it moved.
      if (newMin < oldMin) {
        if (wideningCounter < WideningDelay) {
          this->lower = Bound::constant(newMin);
          incrementCounter = true;
        } else {
          this->lower = Bound::minusInfinity();
        }
      } else {
        this->lower = Bound::constant(oldMin);
      }

      // Upper bound widens only if it moved.
      if (newMax > oldMax) {
        if (wideningCounter < WideningDelay) {
          this->upper = Bound::constant(newMax);
          incrementCounter = true;
        } else {
          this->upper = Bound::plusInfinity();
        }
      } else {
        this->upper = Bound::constant(oldMax);
      }

      // Compute stride from the merged values.
      int base = *merged.begin();
      int g = 0;
      for (int v : merged) {
          if (v == base) {
              continue;
          }
          g = std::gcd(g, v - base);
      }

      this->stride = (g == 0) ? 1 : static_cast<unsigned>(g);

      this->values.clear();
    }
    if (incrementCounter)
      ++wideningCounter;
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
        int base = *this->values.begin();
        int current_gcd = 0;
        for (int v : this->values) {
            if (v == base) {
                continue;
            }
            current_gcd = std::gcd(current_gcd, v - base);
        }
        this->lower = Bound::constant(*this->values.begin());
        this->upper = Bound::constant(*this->values.rbegin());
        this->stride = (current_gcd == 0) ? 1 : static_cast<unsigned>(current_gcd);
        this->kind = Kind::StridedInterval;
        this->values.clear();
    }

  // Now 'this' is definitely a StridedInterval. We process the elements of
  // 'other'.
  if (other.kind == Kind::Set) {
    std::vector<int> vals;
    for (int val : other.values) {
      vals.emplace_back(val);
    }
    this->addConstant(vals);
  } else {
    // Both are Strided Intervals: Merge the interval boundaries

    // Compute Lower Bound
    if (other.lower.isMinusInfinity()) {
      this->lower = Bound::minusInfinity();
    } else if (this->lower.isConstant()) {
      auto thisLowerValue = this->lower.getConstant();
      auto otherLowerValue = other.lower.getConstant();
      this->lower = Bound::constant(std::min(thisLowerValue, otherLowerValue));
    }

    // Compute Upper Bound
    if (other.upper.isPlusInfinity()) {
      this->upper = Bound::plusInfinity();
    } else if (this->upper.isConstant()) {
      auto thisUpperValue = this->upper.getConstant();
      auto otherUpperValue = other.upper.getConstant();
      this->upper = Bound::constant(std::max(thisUpperValue, otherUpperValue));
    }

    // The stride must decrease to capture the strides of both intervals,
    // as well as the alignment offset between their starting configurations.
    if (this->lower.isConstant() && other.lower.isConstant()) {
      int offset = std::abs(this->lower.getConstant() - other.lower.getConstant());
      this->stride = std::gcd(std::gcd(this->stride, other.stride),
                              static_cast<unsigned>(offset));
    } else {
      this->stride = std::gcd(this->stride, other.stride);
    }
  }
}

// Let's work with at most four constants per abstract state. If later on we
// want more precision, then change this definition and recompile.
using AnalyzedValue = AbstractValue<4>;
