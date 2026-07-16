/**
 * @file Bound.h
 * @brief Defines the Bound struct implementing the boundary point
 * of a strided interval, handling infinities.
 */

#pragma once

#include <iostream>

/**
 * @struct Bound
 * @brief Represents a boundary point of a strided interval, handling
 * infinities.
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

  Type type; /**< The type of the bound. */
  int value; /**< The literal value if type is Type::Constant. */

  /**
   * @brief Equality operator for two Bounds.
   */
  friend bool operator==(const Bound &lhs, const Bound &rhs) {
    if (lhs.type != rhs.type)
      return false;
    if (lhs.type == Bound::Type::Constant) {
      return lhs.value == rhs.value;
    }
    return true; // Both are either PlusInfinity or MinusInfinity
  }

  /**
   * @brief Inequality operator for two Bounds.
   */
  friend bool operator!=(const Bound &lhs, const Bound &rhs) {
    return !(lhs == rhs);
  }

  /**
   * @brief Less-than operator for two Bounds.
   */
  friend bool operator<(const Bound &lhs, const Bound &rhs) {
    if (lhs.type == rhs.type) {
      if (lhs.type == Bound::Type::Constant) {
        return lhs.value < rhs.value;
      }
      return false; // Both are -Infinity or both are +Infinity
    }
    // Handle distinct types
    if (lhs.type == Bound::Type::MinusInfinity)
      return true;
    if (lhs.type == Bound::Type::PlusInfinity)
      return false;
    // lhs is Constant
    return rhs.type == Bound::Type::PlusInfinity;
  }

  friend bool operator<=(const Bound &lhs, const Bound &rhs) {
    return (lhs < rhs) || (lhs == rhs);
  }

  friend bool operator>(const Bound &lhs, const Bound &rhs) {
    return rhs < lhs;
  }

  friend bool operator>=(const Bound &lhs, const Bound &rhs) {
    return !(lhs < rhs);
  }

  /**
   * @brief Overload for printing individual boundaries (e.g., -inf, 42, +inf).
   */
  friend std::ostream& operator<<(std::ostream& os, const Bound& bound) {
    switch (bound.type) {
      case Bound::Type::MinusInfinity: os << "-inf"; break;
      case Bound::Type::PlusInfinity:  os << "+inf"; break;
      case Bound::Type::Constant:      os << bound.value; break;
    }
    return os;
  }
};