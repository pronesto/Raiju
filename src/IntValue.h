#pragma once

#include "AbstractValue.h"

template <unsigned N>
class IntValue : public AbstractValue<int, N> {
public:
    void join(const IntValue<N> &other);
    void addConstant(const std::vector<int> &vals) override;
};

template <unsigned N>
void IntValue<N>::addConstant(const std::vector<int> &vals) {
  // If there's no constant to add, we can ignore it
  if (vals.empty()) return;

  bool incrementCounter = false;

  if (this->kind == AbstractValue<int,N>::Kind::Set) {
    for (int val : vals)
        if (this->values.emplace(val).second) incrementCounter = true;

    // Assign bounds based on the captured set bounds
    this->lower = Bound::constant(*this->values.begin());
    this->upper = Bound::constant(*this->values.rbegin());

    // Check if we have exceeded the exact tracking capacity N
    if (this->values.size() > N) {
      // Collapse the representation into a Strided Interval
      this->kind = AbstractValue<int,N>::Kind::StridedInterval;

      int base = *this->values.begin();
      int g = 0;
      for (int v : this->values)
        if (v != base)
          g = std::gcd(g, v - base);

      this->stride = (g == 0) ? 1 : static_cast<unsigned>(g);

      this->values.clear();
    }
  } else {
    for (int val : vals) {
      // If it's already a Strided Interval, we apply the widening logic
      // to adapt the bounds and recalculate the stride based on the new point.
      if (this->lower.isConstant() && val < this->lower.getConstant()) {
        // Case 2: Constant is smaller than the minimum
        if (this->wideningCounter < this->WideningDelay) {
          if (this->upper.isConstant()) {
            this->stride = std::gcd(this->stride, static_cast<unsigned>(std::abs(val - this->upper.getConstant())));
          } else {
            this->stride = 1;
          }
          this->lower = Bound::constant(val);
          incrementCounter = true;
        } else {
          this->lower = Bound::minusInfinity();
          this->stride = 1;
        }
      } else if (this->upper.isConstant() && val > this->upper.getConstant()) {
        // Case 3: Constant is larger than the maximum
        if (this->wideningCounter < this->WideningDelay) {
          if (this->lower.isConstant()) {
            this->stride = std::gcd(this->stride, static_cast<unsigned>(std::abs(val - this->lower.getConstant())));
          } else {
            this->stride = 1;
          }
          this->upper = Bound::constant(val);
          incrementCounter = true;
        } else {
          this->upper = Bound::plusInfinity();
          this->stride = 1;
        }
      } else {
        // Case 1: Inside the current hull bounds
        if (this->lower.isConstant()) {
          this->stride = std::gcd(this->stride, static_cast<unsigned>(std::abs(val - this->lower.getConstant())));
        } else {
          this->stride = 1;
        }
      }
    }
  }
  if (incrementCounter)
    ++this->wideningCounter;
}

template <unsigned N>
void IntValue<N>::join(const IntValue<N> &other) {
  // 1. Both are Sets
  if (this->kind == AbstractValue<int,N>::Kind::Set &&
      other.getKind() == AbstractValue<int,N>::Kind::Set) {

    std::set<int> merged;

    // Take the sorted union of both sets
    std::set_union(this->values.begin(), this->values.end(),
                   other.getValues().begin(), other.getValues().end(),
                   std::inserter(merged, merged.begin()));

    bool incrementCounter = false;
    if (merged.size() <= N) {
      this->values = std::move(merged);
    } else {
      this->kind = AbstractValue<int,N>::Kind::StridedInterval;

      int oldMin = *this->values.begin();
      int oldMax = *this->values.rbegin();

      int newMin = *merged.begin();
      int newMax = *merged.rbegin();

      // Lower bound widens only if it moved.
      if (newMin < oldMin) {
        if (this->wideningCounter < this->WideningDelay) {
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
        if (this->wideningCounter < this->WideningDelay) {
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
      ++this->wideningCounter;
    return;
  }

    // 2. Handling mixed or dual Strided Interval states
    // Force 'this' to adapt to an interval configuration if it's currently a set
    if (this->kind == AbstractValue<int,N>::Kind::Set) {
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
        this->kind = AbstractValue<int,N>::Kind::StridedInterval;
        this->values.clear();
    }

  // Now 'this' is definitely a StridedInterval. We process the elements of
  // 'other'.
  if (other.getKind() == AbstractValue<int,N>::Kind::Set) {
    std::vector<int> vals;
    for (int val : other.getValues()) {
      vals.emplace_back(val);
    }
    this->addConstant(vals);
  } else {
    // Both are Strided Intervals: Merge the interval boundaries

    // Compute Lower Bound
    if (other.getLower().isMinusInfinity()) {
      this->lower = Bound::minusInfinity();
    } else if (this->lower.isConstant()) {
      auto thisLowerValue = this->lower.getConstant();
      auto otherLowerValue = other.getLower().getConstant();
      this->lower = Bound::constant(std::min(thisLowerValue, otherLowerValue));
    }

    // Compute Upper Bound
    if (other.getUpper().isPlusInfinity()) {
      this->upper = Bound::plusInfinity();
    } else if (this->upper.isConstant()) {
      auto thisUpperValue = this->upper.getConstant();
      auto otherUpperValue = other.getUpper().getConstant();
      this->upper = Bound::constant(std::max(thisUpperValue, otherUpperValue));
    }

    // The stride must decrease to capture the strides of both intervals,
    // as well as the alignment offset between their starting configurations.
    if (this->lower.isConstant() && other.getLower().isConstant()) {
      int offset = std::abs(this->lower.getConstant() - other.getLower().getConstant());
      this->stride = std::gcd(std::gcd(this->stride, other.getStride()),
                              static_cast<unsigned>(offset));
    } else {
      this->stride = std::gcd(this->stride, other.getStride());
    }
  }
}

using AnalyzedValue = IntValue<4>;