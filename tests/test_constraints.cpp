#include <catch2/catch_test_macros.hpp>
#include "Constraint.h"

TEST_CASE("Constraints - InitializationConstraint Behavior", "[constraints]") {
    AbstractState state;

    // A variable not yet initialized acts as an empty set (the bottom element)
    // inside a fresh std::unordered_map lookup.
    REQUIRE(state["x0"].getKind() == AnalyzedValue::Kind::Set);

    // Bind x0 = 42
    InitializationConstraint init_x0("x0", 42);

    SECTION("First evaluation updates the state and returns true") {
        bool changed = init_x0.eval(state);
        
        REQUIRE(changed == true);
        REQUIRE(state["x0"].getKind() == AnalyzedValue::Kind::Set);
        
        // Running a quick self-join test to prove 42 is tracked inside the set
        AnalyzedValue expected;
        expected.addConstant(42);
        REQUIRE(state["x0"] == expected);
    }

    SECTION("Subsequent evaluations return false if the value hasn't changed") {
        init_x0.eval(state); // First run (changes state)
        
        bool changed_again = init_x0.eval(state); // Second run
        REQUIRE(changed_again == false);
    }
}

TEST_CASE("Constraints - PhiConstraint Behavior", "[constraints]") {
    AbstractState state;

    // Set up mock incoming variables for an SSA merge:
    // x1 = {3}, x2 = {5}
    InitializationConstraint init_x1("x1", 3);
    InitializationConstraint init_x2("x2", 5);
    init_x1.eval(state);
    init_x2.eval(state);

    // x0 = phi(x1, x2)
    PhiConstraint phi_x0("x0", {"x1", "x2"});

    SECTION("Phi constraint successfully joins multiple operands") {
        bool changed = phi_x0.eval(state);
        
        REQUIRE(changed == true);
        REQUIRE(state["x0"].getKind() == AnalyzedValue::Kind::Set);

        // Expect x0 to hold the union {3, 5}
        AnalyzedValue expected;
        expected.addConstant(3);
        expected.addConstant(5);
        REQUIRE(state["x0"] == expected);
    }

    SECTION("Phi constraint forces collapse into StridedInterval if combined capacity > N") {
        // Let's force-add unique constants to overflow the threshold (N = 4)
        InitializationConstraint init_a("a", 10);
        InitializationConstraint init_b("b", 20);
        InitializationConstraint init_c("c", 30);
        InitializationConstraint init_d("d", 40);
        InitializationConstraint init_e("e", 50);
        
        init_a.eval(state);
        init_b.eval(state);
        init_c.eval(state);
        init_d.eval(state);
        init_e.eval(state);

        // phi_overflow = phi(a, b, c, d, e) -> total 5 elements, triggers collapse
        PhiConstraint phi_overflow("phi_overflow", {"a", "b", "c", "d", "e"});
        
        phi_overflow.eval(state);
        REQUIRE(state["phi_overflow"].getKind() == AnalyzedValue::Kind::StridedInterval);
    }
}
