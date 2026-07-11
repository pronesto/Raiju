/**
 * @file Solver.cpp
 * @brief Definition of the Solver class member functions.
 */

#include "Solver.h"

Solver::Solver(AbstractState &state) : state(state) {}

void Solver::addConstraint(std::shared_ptr<Constraint> constraint) {
  constraints.push_back(constraint);
}

void Solver::clear(){
   constraints.clear();
}

void Solver::resolveSCC() {
  std::cout << "\nSolver called for the following constraints:\n";
  for (auto &constraint : this->constraints) {
      std::cout << "\t" << constraint << "\n";
  }

  std::cout << "Solver state:\n";
  for (auto [var, val] : state) {
    std::cout << "\t" << var << " = " << val << "\n"; 
  }
  std::cout << "\n";

  growthAnalysis();
  futureResolution();
  narrowingAnalysis();
  clear();
}

void Solver::solve(std::vector<std::vector<std::shared_ptr<Constraint>>>& sccs) {
   for (auto &scc : sccs) {
      for (auto &constraint : scc) {
         addConstraint(constraint);
      }
      resolveSCC();
   }
}

void Solver::growthAnalysis() {
  bool changed_evaluating = true;
  int iteration = 0;

  while (changed_evaluating) {
    changed_evaluating = false;
    std::cout << "\n--- Growth Iteration " << ++iteration << " ---\n";

    for (auto &constraint : constraints) {
      if (constraint->eval(this->state)) {
        std::cout << "Growth changed: " << constraint->def << ":"
          << state[constraint->def] << "\n";
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
        std::cout << "Narrowing changed: " << constraint->def << ":"
          << state[constraint->def] << "\n";
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
