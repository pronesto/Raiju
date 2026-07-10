/**
 * @file Solver.cpp
 * @brief Definition of the Solver class member functions.
 */

#include "Solver.h"

Solver::Solver(AbstractState &state) : state(state) {}

void Solver::addConstraint(std::shared_ptr<Constraint> constraint) {
  constraints.push_back(constraint);
}

void Solver::solve() {
  growthAnalysis();
  futureResolution();
  narrowingAnalysis();
}

void Solver::growthAnalysis() {
  bool changed_evaluating = true;
  int iteration = 0;

  while (changed_evaluating) {
    changed_evaluating = false;
    std::cout << "\n--- Growth Iteration " << ++iteration << " ---\n";

    for (auto &constraint : constraints) {
      if (constraint->eval(this->state)) {
        std::cout << "Growth changed: " << constraint->variable_name << ":"
          << state[constraint->variable_name] << "\n";
        changed_evaluating = true;
      }
    }

    // Print the state of your variables. That's just for debugging. We can
    // remove that later on.
    for (auto const& [var, val] : state) {
      std::cout << var << " = " << val << "\n";
    }
  }
}

void Solver::narrowingAnalysis() {
  bool changed_narrowing = true;

  while (changed_narrowing) {
    changed_narrowing = false;

    for (auto &constraint : constraints) {
      if (constraint->narrow(this->state)) {
        std::cout << "Narrowing changed: " << constraint->variable_name << ":"
          << state[constraint->variable_name] << "\n";
        changed_narrowing = true;
      }
    }
  }
}

void Solver::futureResolution() {
  for (auto &constraint : constraints) {
    auto intersection =
        std::dynamic_pointer_cast<IntersectionConstraint>(constraint);

    if (!intersection)
      continue;

    constraint = std::make_shared<IntersectionConstraint>(
        intersection->resolveFutures(state));
  }
}
