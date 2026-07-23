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
template <typename T, unsigned N>
class AbstractValue {
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

protected:
    Kind kind = Kind::Set;                   /**< A Set or a StridedInterval. */
    std::set<T> values;                      /**< Constants when kind == Kind::Set. Unused otherwise. */
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

  virtual ~AbstractValue() = default;

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
  // virtual void join(const AbstractValue &other) = 0;

  /**
   * @brief Add literal constants into the abstract value representation.
   * * @param vals The integer constants to add.
   */
  virtual void addConstant(const std::vector<T> &vals) = 0;

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
  const std::set<T>& getValues() const {
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
