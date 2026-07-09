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
      AbstractState stateBeforeEval = this->state;
      bool is_eval = constraint->eval(this->state);

      if (is_eval) {
        AbstractState stateAfterEval = this->state;
        if (constraint->narrow(stateBeforeEval)) {
          changed_narrowing = true;
        }
      }
    }
  }
}

void Solver::futureResolution() {
  /*
  for (auto &[var_name, val] : state) {
    // Corrected 'var' to 'val' matching your structured binding reference
    if (val.hasFutureBound()) {
      std::string target = val.getFutureTarget();
      AnalyzedValue targetValue = state[target];
      AnalyzedValue concrete = targetValue.addOffset(val.getFutureOffset());
      // TODO: Apply the concrete value back into the state if necessary
    }
  }
  */
}
