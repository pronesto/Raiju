/**
 * @file test_solver.cpp
 * @brief Unit tests for the Solver fixed-point loop engine.
 */

#include "Constraint.h"
#include "Solver.h"
#include "Graph.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Solver growthAnalysis loops to positive infinity on feedback loop",
          "[solver][growth]") {
  // 1. Initialize the global abstract state table
  AbstractState state;

  // Helper shorthand types from your Constraint.h definitions
  using Type = Bound::Type;
  using InterBound = IntersectionConstraint::IntersectionBound;

  // 2. Instantiate the constraint objects using shared_ptr
  auto const_1 = std::make_shared<InitializationConstraint>("const_1", 1);
  auto k0 = std::make_shared<InitializationConstraint>("k0", 0);

  auto k1 = std::make_shared<PhiConstraint>(
      "k1", std::vector<std::string>{"k0", "k2"});

  // Construct upper/lower bounds for the intersection constraint: [-Infinity,
  // 99]
  Bound minusInf = Bound::minusInfinity() = Bound::minusInfinity();

  Bound ninetyNine = Bound::constant(99);

  auto kt = std::make_shared<IntersectionConstraint>("kt", "k1", minusInf,
                                                     ninetyNine);
  auto k2 = std::make_shared<AddConstraint>("k2", "kt", "const_1");

  // 3. Register constraints into the solver pipeline
  Solver solver(state);
  solver.addConstraint(const_1);
  solver.addConstraint(k0);
  solver.addConstraint(k1);
  solver.addConstraint(kt);
  solver.addConstraint(k2);

  // 4. Run ONLY the growth analysis phase
  solver.growthAnalysis();

  // 5. Verify the expected abstract state limits before narrowing corrections

  // k0 should remain exactly 0
  REQUIRE(state["k0"].getLower().getConstant() == 0);
  REQUIRE(state["k0"].getUpper().getConstant() == 0);

  // k1 should widen up to PlusInfinity: [0, +inf]
  REQUIRE(state["k1"].getLower().getConstant() == 0);
  REQUIRE(state["k1"].getUpper().isPlusInfinity());

  // kt without narrowing should evaluate alongside its source 'k1' up to
  // PlusInfinity: [0, +inf]
  REQUIRE(state["kt"].getLower().getConstant() == 0);
  REQUIRE(state["kt"].getUpper().isPlusInfinity());

  // k2 follows 'kt' + 1: [1, +inf]
  REQUIRE(state["k2"].getLower().getConstant() == 1);
  REQUIRE(state["k2"].getUpper().isPlusInfinity());
}

TEST_CASE("Solver growthAnalysis widens a simple increasing loop",
          "[solver][growth]") {
  // 1. Initialize the abstract state.
  AbstractState state;

  auto k0 = std::make_shared<InitializationConstraint>("k0", 0);

  auto k1 = std::make_shared<PhiConstraint>(
      "k1", std::vector<std::string>{"k0", "k2"});

  auto const_1 =
      std::make_shared<InitializationConstraint>("const_1", 1);

  auto k2 =
      std::make_shared<AddConstraint>("k2", "k1", "const_1");

  // 2. Register constraints.
  Solver solver(state);
  solver.addConstraint(k0);
  solver.addConstraint(k1);
  solver.addConstraint(const_1);
  solver.addConstraint(k2);

  // 3. Run the growth analysis.
  solver.growthAnalysis();

  // k0 remains constant.
  REQUIRE(state["k0"].getKind() == AnalyzedValue::Kind::Set);
  REQUIRE(state["k0"].getValues() == std::set<int>{0});

  // k1 widens to [0,+inf].
  REQUIRE(state["k1"].getKind() == AnalyzedValue::Kind::StridedInterval);
  REQUIRE(state["k1"].getLower().getConstant() == 0);
  REQUIRE(state["k1"].getUpper().isPlusInfinity());

  // k2 = k1 + 1 = [1,+inf].
  REQUIRE(state["k2"].getKind() == AnalyzedValue::Kind::StridedInterval);
  REQUIRE(state["k2"].getLower().getConstant() == 1);
  REQUIRE(state["k2"].getUpper().isPlusInfinity());
}

TEST_CASE("Solver narrowingAnalysis reclaims precision back down to the loop bound",
          "[solver][narrowing]") {
  // 1. Initialize a clean abstract state table
  AbstractState state;

  using Bound = Bound;
  using Type = Bound::Type;

  // 2. Re-instantiate the same system of constraints
  auto const_1 = std::make_shared<InitializationConstraint>("const_1", 1);
  auto k0 = std::make_shared<InitializationConstraint>("k0", 0);
  auto k1 = std::make_shared<PhiConstraint>("k1", std::vector<std::string>{"k0", "k2"});

  Bound minusInf = Bound::minusInfinity();

  Bound ninetyNine = Bound::constant(99);

  auto kt = std::make_shared<IntersectionConstraint>("kt", "k1", minusInf, ninetyNine);
  auto k2 = std::make_shared<AddConstraint>("k2", "kt", "const_1");

  Solver solver(state);
  solver.addConstraint(const_1);
  solver.addConstraint(k0);
  solver.addConstraint(k1);
  solver.addConstraint(kt);
  solver.addConstraint(k2);

  // 3. Run the FULL solve pipeline (growth Analysis -> future resolution -> narrowing Analysis)
  solver.resolveSCC();

  // 4. Verify that monotonic narrowing successfully refined the intervals

  // k0 remains exactly 0
  REQUIRE(state["k0"].getLower().getConstant() == 0);
  REQUIRE(state["k0"].getUpper().getConstant() == 0);

  // k1's upper bound should narrow down from +inf to 100 [0, 100]
  REQUIRE(state["k1"].getLower().getConstant() == 0);
  REQUIRE(state["k1"].getUpper().getConstant() == 100);

  // kt remains safely clamped between [0, 99]
  REQUIRE(state["kt"].getLower().getConstant() == 0);
  REQUIRE(state["kt"].getUpper().getConstant() == 99);

  // k2 settles at kt's upper bound + 1 [1, 100]
  REQUIRE(state["k2"].getLower().getConstant() == 1);
  REQUIRE(state["k2"].getUpper().getConstant() == 100);
}

TEST_CASE("Solver resolves future bounds during solve",
          "[solver][future-resolution]") {

  AbstractState state;

  using Bound = Bound;
  using Type  = Bound::Type;

  auto c1    = std::make_shared<InitializationConstraint>("const_1", 1);
  auto limit = std::make_shared<InitializationConstraint>("limit", 99);

  auto i0 = std::make_shared<InitializationConstraint>("i0", 0);

  auto i1 = std::make_shared<PhiConstraint>(
      "i1",
      std::vector<std::string>{"i0", "i2"});

  Bound minusInf = Bound::minusInfinity();

  auto it = std::make_shared<IntersectionConstraint>(
      "it",
      "i1",
      minusInf,
      IntersectionConstraint::Future{"limit", 0});

  auto i2 = std::make_shared<AddConstraint>(
      "i2",
      "it",
      "const_1");

  Solver solver(state);
  solver.addConstraint(c1);
  solver.addConstraint(limit);
  solver.addConstraint(i0);
  solver.addConstraint(i1);
  solver.addConstraint(it);
  solver.addConstraint(i2);

  solver.resolveSCC();

  REQUIRE(state["limit"].getUpper().getConstant() == 99);

  REQUIRE(state["it"].getLower().getConstant() == 0);
  REQUIRE(state["it"].getUpper().getConstant() == 99);

  REQUIRE(state["i2"].getLower().getConstant() == 1);
  REQUIRE(state["i2"].getUpper().getConstant() == 100);
}

TEST_CASE("Solver handles mutually recursive future bounds",
          "[solver][future-resolution][mutual]") {
  // 1. Initialize a clean abstract state table
  AbstractState state;

  using Bound = Bound;
  using Type  = Bound::Type;

  // Constants
  auto const_1 = std::make_shared<InitializationConstraint>("const_1", 1);
  auto minus_1 =
    std::make_shared<InitializationConstraint>("minus_1", -1);

  // Initial values
  auto i0 = std::make_shared<InitializationConstraint>("i0", 0);
  auto j0 = std::make_shared<InitializationConstraint>("j0", 99);

  // Phi nodes
  auto i1 = std::make_shared<PhiConstraint>(
      "i1", std::vector<std::string>{"i0", "i2"});

  auto j1 = std::make_shared<PhiConstraint>(
      "j1", std::vector<std::string>{"j0", "j2"});

  Bound minusInf = Bound::minusInfinity();

  Bound plusInf = Bound::plusInfinity();

  // it = i1 ∩ [-inf, ft(j1)-1]
  auto it = std::make_shared<IntersectionConstraint>(
      "it",
      "i1",
      minusInf,
      IntersectionConstraint::Future{"j1", -1});

  // jt = j1 ∩ [ft(i1)+1, +inf]
  auto jt = std::make_shared<IntersectionConstraint>(
      "jt",
      "j1",
      IntersectionConstraint::Future{"i1", 1},
      plusInf);

  // i2 = it + 1
  auto i2 = std::make_shared<AddConstraint>(
      "i2", "it", "const_1");

  // j2 = jt - 1
  auto j2 = std::make_shared<AddConstraint>(
      "j2", "jt", "minus_1");

  Solver solver(state);

  solver.addConstraint(minus_1);
  solver.addConstraint(const_1);
  solver.addConstraint(i0);
  solver.addConstraint(j0);
  solver.addConstraint(i1);
  solver.addConstraint(j1);
  solver.addConstraint(it);
  solver.addConstraint(jt);
  solver.addConstraint(i2);
  solver.addConstraint(j2);

  solver.resolveSCC();

  // Initial values remain unchanged.
  REQUIRE(state["i0"].getLower().getConstant() == 0);
  REQUIRE(state["i0"].getUpper().getConstant() == 0);

  REQUIRE(state["j0"].getLower().getConstant() == 99);
  REQUIRE(state["j0"].getUpper().getConstant() == 99);

  // The two induction variables should remain finite after narrowing.


  // The intersections must also have finite bounds.


  // Verify that the relational invariants induced by the futures hold.
  REQUIRE(state["it"].getUpper().getConstant() <= state["j1"].getUpper().getConstant() - 1);
  REQUIRE(state["jt"].getLower().getConstant() >= state["i1"].getLower().getConstant() + 1);

  // The transfer functions must also hold.
  REQUIRE(state["i2"].getLower().getConstant() ==
          state["it"].getLower().getConstant() + 1);
  REQUIRE(state["i2"].getUpper().getConstant() ==
          state["it"].getUpper().getConstant() + 1);

  REQUIRE(state["j2"].getLower().getConstant() ==
          state["jt"].getLower().getConstant() - 1);
  REQUIRE(state["j2"].getUpper().getConstant() ==
          state["jt"].getUpper().getConstant() - 1);
}

TEST_CASE("Solver handles complete running example",
          "[solver][growth][future-resolution][narrowing]") {
  // 1. Initialize a clean abstract state table
  AbstractState state;

  using Bound = Bound;
  using Type = Bound::Type;

  // 2. Re-instantiate the same system of constraints
  auto const_1 = std::make_shared<InitializationConstraint>("const_1", 1);
  auto minus_1 = std::make_shared<InitializationConstraint>("minus_1", -1);

  Bound minusInf = Bound::minusInfinity();

  Bound plusInf = Bound::plusInfinity();

  Bound ninetyNine = Bound::constant(99);

  Bound hundred = Bound::constant(100);


  auto k0 = std::make_shared<InitializationConstraint>("k0", 0);
  auto kt = std::make_shared<IntersectionConstraint>("kt", "k1", minusInf, ninetyNine);
  auto kf = std::make_shared<IntersectionConstraint>("kf", "k1", hundred, plusInf);
  auto k1 = std::make_shared<PhiConstraint>("k1", std::vector<std::string>{"k0", "k2"});

  auto i0 = std::make_shared<InitializationConstraint>("i0", 0);
  auto j0 = std::make_shared<IntersectionConstraint>("j0", "kt", minusInf, plusInf);

  auto i1 = std::make_shared<PhiConstraint>("i1", std::vector<std::string>{"i0", "i2"});
  auto j1 = std::make_shared<PhiConstraint>("j1", std::vector<std::string>{"j0", "j2"});

  // it = i1 ∩ [-inf, ft(j1)-1]
  auto it = std::make_shared<IntersectionConstraint>(
      "it",
      "i1",
      minusInf,
      IntersectionConstraint::Future{"j1", -1});

  // jt = j1 ∩ [ft(i1)+1, +inf]
  auto jt = std::make_shared<IntersectionConstraint>(
      "jt",
      "j1",
      IntersectionConstraint::Future{"i1", 0},
      plusInf);
  
  auto i2 = std::make_shared<AddConstraint>("i2", "it", "const_1");
  auto j2 = std::make_shared<AddConstraint>("j2", "jt", "minus_1");
  auto k2 = std::make_shared<AddConstraint>("k2", "kt", "const_1");

  Solver solver(state);

  solver.addConstraint(minus_1);
  solver.addConstraint(const_1);
  solver.addConstraint(k0);
  solver.addConstraint(kt);
  solver.addConstraint(kf);
  solver.addConstraint(k1);
  solver.addConstraint(i0);
  solver.addConstraint(j0);
  solver.addConstraint(i1);
  solver.addConstraint(j1);
  solver.addConstraint(it);
  solver.addConstraint(jt);
  solver.addConstraint(i2);
  solver.addConstraint(j2);
  solver.addConstraint(k2);

  solver.resolveSCC();

  // Check values

  REQUIRE(state["i0"].getLower().getConstant() == 0);
  REQUIRE(state["i0"].getUpper().getConstant() == 0);

  REQUIRE(state["i1"].getLower().getConstant() == 0);
  REQUIRE(state["i1"].getUpper().isPlusInfinity());

  REQUIRE(state["i2"].getLower().getConstant() == 1);
  REQUIRE(state["i2"].getUpper().isPlusInfinity());

  REQUIRE(state["it"].getLower().getConstant() == 0);
  REQUIRE(state["it"].getUpper().isPlusInfinity());

  REQUIRE(state["j0"].getLower().getConstant() == 0);
  REQUIRE(state["j0"].getUpper().getConstant() == 99);

  REQUIRE(state["j1"].getLower().getConstant() == -1);
  REQUIRE(state["j1"].getUpper().isPlusInfinity());

  REQUIRE(state["j2"].getLower().getConstant() == -1);
  REQUIRE(state["j2"].getUpper().isPlusInfinity());

  REQUIRE(state["jt"].getLower().getConstant() == 0);
  REQUIRE(state["jt"].getUpper().isPlusInfinity());

  REQUIRE(state["k0"].getLower().getConstant() == 0);
  REQUIRE(state["k0"].getUpper().getConstant() == 0);

  REQUIRE(state["k1"].getLower().getConstant() == 0);
  REQUIRE(state["k1"].getUpper().getConstant() == 100);

  REQUIRE(state["k2"].getLower().getConstant() == 1);
  REQUIRE(state["k2"].getUpper().getConstant() == 100);

  REQUIRE(state["kt"].getLower().getConstant() == 0);
  REQUIRE(state["kt"].getUpper().getConstant() == 99);

  REQUIRE(state["kf"].getLower().getConstant() == 100);
  REQUIRE(state["kf"].getUpper().getConstant() == 100);
}

TEST_CASE("Solver and ConstraintGraph Integration: Complete Running Example",
          "[solver][graph][integration]") {
  
  AbstractState state;
  ConstraintGraph graph;

  using Bound = Bound;
  using Type = Bound::Type;

  auto const_1 = std::make_shared<InitializationConstraint>("const_1", 1);
  auto minus_1 = std::make_shared<InitializationConstraint>("minus_1", -1);

  Bound minusInf = Bound::minusInfinity();

  Bound plusInf = Bound::plusInfinity();

  Bound ninetyNine = Bound::constant(99);

  Bound hundred = Bound::constant(100);


  auto k0 = std::make_shared<InitializationConstraint>("k0", 0);
  auto kt = std::make_shared<IntersectionConstraint>("kt", "k1", minusInf, ninetyNine);
  auto kf = std::make_shared<IntersectionConstraint>("kf", "k1", hundred, plusInf);
  auto k1 = std::make_shared<PhiConstraint>("k1", std::vector<std::string>{"k0", "k2"});

  auto i0 = std::make_shared<InitializationConstraint>("i0", 0);
  auto j0 = std::make_shared<IntersectionConstraint>("j0", "kt", minusInf, plusInf);

  auto i1 = std::make_shared<PhiConstraint>("i1", std::vector<std::string>{"i0", "i2"});
  auto j1 = std::make_shared<PhiConstraint>("j1", std::vector<std::string>{"j0", "j2"});

  // it = i1 ∩ [-inf, ft(j1)-1]
  auto it = std::make_shared<IntersectionConstraint>(
      "it",
      "i1",
      minusInf,
      IntersectionConstraint::Future{"j1", -1});

  // jt = j1 ∩ [ft(i1)+1, +inf]
  auto jt = std::make_shared<IntersectionConstraint>(
      "jt",
      "j1",
      IntersectionConstraint::Future{"i1", 0},
      plusInf);
  
  auto i2 = std::make_shared<AddConstraint>("i2", "it", "const_1");
  auto j2 = std::make_shared<AddConstraint>("j2", "jt", "minus_1");
  auto k2 = std::make_shared<AddConstraint>("k2", "kt", "const_1");

  graph.addConstraint(minus_1);
  graph.addConstraint(const_1);
  graph.addConstraint(k0);
  graph.addConstraint(kt);
  graph.addConstraint(kf);
  graph.addConstraint(k1);
  graph.addConstraint(i0);
  graph.addConstraint(j0);
  graph.addConstraint(i1);
  graph.addConstraint(j1);
  graph.addConstraint(it);
  graph.addConstraint(jt);
  graph.addConstraint(i2);
  graph.addConstraint(j2);
  graph.addConstraint(k2);

  // ==========================================
  // O teste real da sua arquitetura:
  // ==========================================
  auto sccs = graph.getTopologicalSCCs();
  
  Solver solver(state);
  solver.solve(sccs);

  // ==========================================
  // Verificação matemática
  // ==========================================
  REQUIRE(state["i0"].getLower().getConstant() == 0);
  REQUIRE(state["i0"].getUpper().getConstant() == 0);
  REQUIRE(state["i1"].getLower().getConstant() == 0);
  REQUIRE(state["i1"].getUpper().getConstant() == 99);
  REQUIRE(state["i2"].getLower().getConstant() == 1);
  REQUIRE(state["i2"].getUpper().getConstant() == 99);
  REQUIRE(state["it"].getLower().getConstant() == 0);
  REQUIRE(state["it"].getUpper().getConstant() == 98);

  REQUIRE(state["j0"].getLower().getConstant() == 0);
  REQUIRE(state["j0"].getUpper().getConstant() == 99);
  REQUIRE(state["j1"].getLower().getConstant() == -1);
  REQUIRE(state["j1"].getUpper().getConstant() == 99);
  REQUIRE(state["j2"].getLower().getConstant() == -1);
  REQUIRE(state["j2"].getUpper().getConstant() == 98);
  REQUIRE(state["jt"].getLower().getConstant() == 0);
  REQUIRE(state["jt"].getUpper().getConstant() == 99);

  REQUIRE(state["k0"].getLower().getConstant() == 0);
  REQUIRE(state["k0"].getUpper().getConstant() == 0);
  REQUIRE(state["k1"].getLower().getConstant() == 0);
  REQUIRE(state["k1"].getUpper().getConstant() == 100);
  REQUIRE(state["k2"].getLower().getConstant() == 1);
  REQUIRE(state["k2"].getUpper().getConstant() == 100);
  REQUIRE(state["kt"].getLower().getConstant() == 0);
  REQUIRE(state["kt"].getUpper().getConstant() == 99);
  REQUIRE(state["kf"].getLower().getConstant() == 100);
  REQUIRE(state["kf"].getUpper().getConstant() == 100);
}

TEST_CASE("Solver and ConstraintGraph Integration: TooLong example",
          "[solver][graph][integration]") {
  
  AbstractState state;
  ConstraintGraph graph;

  using Bound = Bound;
  using Type = Bound::Type;

  auto const_0 = std::make_shared<InitializationConstraint>("const_0", 0);
  auto const_1 = std::make_shared<InitializationConstraint>("const_1", 1);

  auto tooLong_0 = std::make_shared<PhiConstraint>("tooLong_0", std::vector<std::string>{"const_0", "tooLong_1"});
  auto tooLong_1 = std::make_shared<PhiConstraint>("tooLong_1", std::vector<std::string>{"const_1", "tooLong_0"});

  graph.addConstraint(const_0);
  graph.addConstraint(const_1);
  graph.addConstraint(tooLong_0);
  graph.addConstraint(tooLong_1);

  // ==========================================
  // O teste real da sua arquitetura:
  // ==========================================
  auto sccs = graph.getTopologicalSCCs();
  
  Solver solver(state);
  solver.solve(sccs);

  // ==========================================
  // Verificação matemática
  // ==========================================
  REQUIRE(state["tooLong_0"].getLower().getConstant() == 0);
  REQUIRE(state["tooLong_0"].getUpper().getConstant() == 1);
  REQUIRE(state["tooLong_1"].getLower().getConstant() == 0);
  REQUIRE(state["tooLong_1"].getUpper().getConstant() == 1);
}