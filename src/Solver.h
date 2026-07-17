/**
 * @file Solver.h
 * @brief Declaration of the Solver class responsible for running fixed-point
 * dataflow analyses.
 */

#ifndef SOLVER_H
#define SOLVER_H

#include "AbstractValue.h"
#include "Constraint.h"
#include <memory>
#include <vector>

/**
 * @class Solver
 * @brief Coordinates the growth, resolution, and narrowing phases of the range
 * analysis solver.
 *
 * This class orchestrates a multi-step abstract interpretation pipeline,
 * driving the system of abstract state constraints to a sound mathematical
 * fixed point.
 */
class Solver {
private:
  /** @brief Reference to the global abstract state table mapping variables to
   * domains. */
  AbstractState &state;

  /** @brief Collection of dataflow/SSA constraint equations to execute. */
  std::vector<std::shared_ptr<Constraint>> constraints;

public:
  /**
   * @brief Constructs a new Solver instance bound to a given abstract state.
   * @param state A reference to the global variable-to-value map tracking the
   * analysis.
   */
  explicit Solver(AbstractState &state);

  /**
   * @brief Registers a new dataflow constraint equation into the solver
   * pipeline.
   * @param constraint Shared pointer to an abstract Constraint subclass.
   */
  void addConstraint(std::shared_ptr<Constraint> constraint);

  /**
   * @brief Main driver that executes all stages of the range analysis pipeline.
   * * Progresses sequentially through the widening/growth phase, symbolic
   * future resolution, and the monotonic narrowing phase until a fixed point is
   * met.
   */
  void resolveSCC();
  void solve(std::vector<std::vector<std::shared_ptr<Constraint>>>& sccs);
  void clear();

  /**
   * @brief Runs the initial growth (widening) analysis loop.
   * * Iteratively evaluates dataflow equations. This step propagates and widens
   * bounds upwards until the values stabilize or are forced to infinity.
   */
  void growthAnalysis();

  /**
   * @brief Resolves symbolic/future boundaries into concrete bounds using
   * current state data.
   * * Iterates through the abstract state map, evaluates relational
   * dependencies between variables (like offset relations), and updates the
   * bounds dynamically.
   */
  void futureResolution();

  /**
   * @brief Runs the narrowing analysis loop to reclaim precision loss.
   * * Iteratively applies monotonic narrowing operators. It evaluates
   * constraints and attempts to contract over-approximated interval boundaries
   * without violating soundness.
   */
  void narrowingAnalysis();
};

#endif // SOLVER_H
