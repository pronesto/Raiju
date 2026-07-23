#pragma once

#include "AbstractValue.h"

class BoolValue : public AbstractValue<bool, 2> {
public:
  void join(const BoolValue &other);
  void addConstant(const std::vector<bool> &vals) override;
};

void BoolValue::addConstant(const std::vector<bool> &vals) {
  // If there's no constant to add, we can ignore it
  if (vals.empty()) return;

  for (int val : vals)
      this->values.emplace(val);

  // Assign bounds based on the captured set bounds
  this->lower = Bound::constant(*this->values.begin());
  this->upper = Bound::constant(*this->values.rbegin());
}

void BoolValue::join(const BoolValue &other) {
  std::set<bool> merged;

  // Take the sorted union of both sets
  std::set_union(this->values.begin(), this->values.end(),
                  other.getValues().begin(), other.getValues().end(),
                  std::inserter(merged, merged.begin()));

  this->values = std::move(merged);
}