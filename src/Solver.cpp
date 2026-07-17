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
  std::cout << "\nState after growth analysis:\n";
  for (auto const& [var, val] : state) {
    std::cout << var << " = " << val << "\n";
  }

  // Make counters useless after growth analysis
  for (auto const& [var, val] : state) {
    state[var].resetCounter();
  }

  futureResolution();
  std::cout << "\nState after future resolution\n";
  for (auto const& [var, val] : state) {
    std::cout << var << " = " << val << "\n";
  }
  narrowingAnalysis();
  std::cout << "\nState after narrowing analysis\n";
  for (auto const& [var, val] : state) {
    std::cout << var << " = " << val << "\n";
  }
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
    ++iteration;

    for (auto &constraint : constraints) {
      if (constraint->eval(this->state)) {
        changed_evaluating = true;
      }
    }
  }
}

void Solver::narrowingAnalysis() {
  bool changed_narrowing = true;

  while (changed_narrowing) {
    changed_narrowing = false;

    for (auto &constraint : constraints) {
      if (constraint->narrow(this->state)) {
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
