/**
 * @file Bound.h
 * @brief Defines the Bound struct implementing the boundary point
 * of a strided interval, handling infinities.
 */

#pragma once

#include <iostream>
#include <limits>
#include <assert.h>

/**
 * @struct Bound
 * @brief Represents a boundary point of a strided interval, handling
 * infinities.
 */
struct Bound {
public:
  /**
   * @enum Type
   * @brief Defines the nature of the interval bound.
   */
  enum class Type {
    MinusInfinity, /**< Represents negative infinity (-\infty). */
    Constant,      /**< Represents a precise integer constant. */
    PlusInfinity   /**< Represents positive infinity (+\infty). */
  };

private:
  constexpr long long toKey() const {
    if (type == Type::MinusInfinity) return std::numeric_limits<long long>::min();
    if (type == Type::PlusInfinity) return std::numeric_limits<long long>::max();
    return static_cast<long long>(value);
  }

  Bound(Type type, int value) : type(type), value(value) {}

public:
  Type type; /**< The type of the bound. */
  int value; /**< The literal value if type is Type::Constant. */
  
  Bound() : type(Type::MinusInfinity), value(0) {}

  static Bound minusInfinity() { return {Type::MinusInfinity, 0}; }
  static Bound plusInfinity()  { return {Type::PlusInfinity, 0}; }
  static Bound constant(int v) { return {Type::Constant, v}; }

  constexpr bool isMinusInfinity() const { return type == Type::MinusInfinity; }
  constexpr bool isPlusInfinity() const { return type == Type::PlusInfinity; }
  constexpr bool isConstant() const { return type == Type::Constant; }
  constexpr int getConstant() const { 
    assert (this->isConstant() && "getConstant() requires a constant Bound");
    return value;
  }

  /**
   * @brief Equality operator for two Bounds.
   */
  friend constexpr bool operator==(const Bound &lhs, const Bound &rhs) {
    return lhs.type == rhs.type &&
      (lhs.type != Type::Constant || lhs.value == rhs.value);
  }

  /**
   * @brief Inequality operator for two Bounds.
   */
  friend constexpr bool operator!=(const Bound &lhs, const Bound &rhs) {
    return !(lhs == rhs);
  }

  /**
   * @brief Less-than operator for two Bounds.
   */
  friend constexpr bool operator<(const Bound &lhs, const Bound &rhs) {
    return lhs.toKey() < rhs.toKey();
  }

  friend constexpr bool operator<=(const Bound &lhs, const Bound &rhs) {
    return (lhs < rhs) || (lhs == rhs);
  }

  friend constexpr bool operator>(const Bound &lhs, const Bound &rhs) {
    return rhs < lhs;
  }

  friend constexpr bool operator>=(const Bound &lhs, const Bound &rhs) {
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