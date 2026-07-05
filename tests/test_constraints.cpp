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

#include <catch2/catch_test_macros.hpp>
#include "Constraint.h"

TEST_CASE("Constraints - AddConstraint Pairwise Sets", "[constraints][add]") {
    AbstractState state;

    // v1 = {2, 3}
    InitializationConstraint init_v1_a("v1", 2);
    init_v1_a.eval(state);
    // Since addConstant or join isn't directly exposed in Constraint,
    // let's simulate a set by using a Phi style layout or a mock update.
    // For test isolation, we'll manually push data into the map or use a custom tool.
    // But since state is an unordered_map, we can populate it directly in the test!

    AnalyzedValue v1;
    v1.addConstant(2);
    v1.addConstant(3);
    state["v1"] = v1;

    // v2 = {10, 20}
    AnalyzedValue v2;
    v2.addConstant(10);
    v2.addConstant(20);
    state["v2"] = v2;

    // v0 = v1 + v2
    AddConstraint add_v0("v0", "v1", "v2");

    SECTION("Exact pairwise addition for sets under capacity") {
        bool changed = add_v0.eval(state);

        REQUIRE(changed == true);
        REQUIRE(state["v0"].getKind() == AnalyzedValue::Kind::Set);

        // Expected unique combinations: 2+10=12, 2+20=22, 3+10=13, 3+20=23
        // Sorted: {12, 13, 22, 23} (Total size 4, which is <= N=4)
        AnalyzedValue expected;
        expected.addConstant(12);
        expected.addConstant(13);
        expected.addConstant(22);
        expected.addConstant(23);

        REQUIRE(state["v0"] == expected);
    }
}

TEST_CASE("Constraints - AddConstraint Overflow and Interval Math", "[constraints][add]") {
    AbstractState state;

    AnalyzedValue v1;
    v1.addConstant(1);
    v1.addConstant(2);
    v1.addConstant(3);
    state["v1"] = v1;

    AnalyzedValue v2;
    v2.addConstant(10);
    v2.addConstant(20);
    state["v2"] = v2;

    AddConstraint add_v0("v0", "v1", "v2");

    SECTION("Addition widens after the finite-set capacity is exceeded") {

        REQUIRE(add_v0.eval(state));

        const auto& result = state["v0"];

        REQUIRE(result.getKind() == AnalyzedValue::Kind::StridedInterval);

        REQUIRE(result.getLower().type ==
                AnalyzedValue::Bound::Type::Constant);
        REQUIRE(result.getLower().value == 11);

        REQUIRE(result.getUpper().type ==
                AnalyzedValue::Bound::Type::PlusInfinity);

        REQUIRE(result.getStride() == 1);
    }
}
