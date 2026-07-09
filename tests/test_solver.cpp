/**
 * @file test_solver.cpp
 * @brief Unit tests for the Solver fixed-point loop engine.
 */

#include <catch2/catch_test_macros.hpp>
#include "Solver.h"
#include "Constraint.h"

TEST_CASE("Solver growthAnalysis loops to positive infinity on feedback loop", "[solver][growth]") {
    // 1. Initialize the global abstract state table
    AbstractState state;

    // Helper shorthand types from your Constraint.h definitions
    using Bound = AnalyzedValue::Bound;
    using Type  = Bound::Type;
    using InterBound = IntersectionConstraint::IntersectionBound;

    // 2. Instantiate the constraint objects using shared_ptr
    auto const_1 = std::make_shared<InitializationConstraint>("const_1", 1);
    auto k0      = std::make_shared<InitializationConstraint>("k0", 0);
    
    auto k1      = std::make_shared<PhiConstraint>("k1", std::vector<std::string>{"k0", "k2"});

    // Construct upper/lower bounds for the intersection constraint: [-Infinity, 99]
    Bound minusInf;
    minusInf.type = Type::MinusInfinity;
    
    Bound ninetyNine;
    ninetyNine.type = Type::Constant;
    ninetyNine.value = 99;

    auto kt      = std::make_shared<IntersectionConstraint>("kt", "k1", minusInf, ninetyNine);
    auto k2      = std::make_shared<AddConstraint>("k2", "kt", "const_1");

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
    REQUIRE(state["k0"].getLower().value == 0);
    REQUIRE(state["k0"].getUpper().value == 0);

    // k1 should widen up to PlusInfinity: [0, +inf]
    INFO("k1 Upper Bound Type: " << (int)state["k1"].getUpper().type);
    INFO("k1 Upper Bound Value: " << state["k1"].getUpper().value);
    REQUIRE(state["k1"].getUpper().type == Type::PlusInfinity);
    REQUIRE(state["k1"].getLower().value == 0);
    REQUIRE(state["k1"].getUpper().type == Type::PlusInfinity);

    // kt without narrowing should evaluate alongside its source 'k1' up to PlusInfinity: [0, +inf]
    REQUIRE(state["kt"].getLower().value == 0);
    REQUIRE(state["kt"].getUpper().type == Type::PlusInfinity);

    // k2 follows 'kt' + 1: [1, +inf]
    REQUIRE(state["k2"].getLower().value == 1);
    REQUIRE(state["k2"].getUpper().type == Type::PlusInfinity);
}
