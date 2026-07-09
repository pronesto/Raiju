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

TEST_CASE("Intersection with constant upper bound", "[constraints][intersect]") {

    AbstractState state;

    AnalyzedValue v;
    v.addConstant(1);
    v.addConstant(5);
    v.addConstant(10);

    state["x"] = v;

    AnalyzedValue::Bound minusInf;
    minusInf.type = AnalyzedValue::Bound::Type::MinusInfinity;

    AnalyzedValue::Bound five;
    five.type = AnalyzedValue::Bound::Type::Constant;
    five.value = 5;

    IntersectionConstraint C("y","x",minusInf,five);

    REQUIRE(C.eval(state));

    REQUIRE(state["y"].getKind()==AnalyzedValue::Kind::Set);

    REQUIRE(state["y"].getValues()==std::vector<int>{1,5});
}

TEST_CASE("Intersection narrows interval", "[constraints][intersect]") {

    AbstractState state;

    AnalyzedValue x;

    AnalyzedValue::Bound low;
    low.type=AnalyzedValue::Bound::Type::Constant;
    low.value=0;

    AnalyzedValue::Bound up;
    up.type=AnalyzedValue::Bound::Type::Constant;
    up.value=100;

    x.setAsInterval(low,up,1);

    state["x"]=x;

    AnalyzedValue::Bound ten;
    ten.type=AnalyzedValue::Bound::Type::Constant;
    ten.value=10;

    AnalyzedValue::Bound twenty;
    twenty.type=AnalyzedValue::Bound::Type::Constant;
    twenty.value=20;

    IntersectionConstraint C("y","x",ten,twenty);

    REQUIRE(C.eval(state));

    REQUIRE(state["y"].getLower().value==10);
    REQUIRE(state["y"].getUpper().value==20);
}

TEST_CASE("Growth phase ignores futures", "[constraints][intersect]") {

    AbstractState state;

    AnalyzedValue x;
    x.addConstant(1);
    x.addConstant(5);
    x.addConstant(10);

    state["x"]=x;

    IntersectionConstraint::Future F{"y", -1};

    AnalyzedValue::Bound minusInf;
    minusInf.type=AnalyzedValue::Bound::Type::MinusInfinity;

    IntersectionConstraint C("z", "x", minusInf, F);

    REQUIRE(C.eval(state));

    REQUIRE(state["z"]==state["x"]);
}

TEST_CASE("Narrow with finite bounds tightening", "[constraints][narrow]") {
    AbstractState state;

    // Start with a wide interval [0, 100]
    AnalyzedValue x;
    AnalyzedValue::Bound low;
    low.type = AnalyzedValue::Bound::Type::Constant;
    low.value = 0;

    AnalyzedValue::Bound up;
    up.type = AnalyzedValue::Bound::Type::Constant;
    up.value = 100;

    x.setAsInterval(low, up, 1);
    state["x"] = x;

    // Create an intersection constraint that should narrow [0,100] to [10,20]
    AnalyzedValue::Bound ten;
    ten.type = AnalyzedValue::Bound::Type::Constant;
    ten.value = 10;

    AnalyzedValue::Bound twenty;
    twenty.type = AnalyzedValue::Bound::Type::Constant;
    twenty.value = 20;

    IntersectionConstraint C("x", "x", ten, twenty);

    // First eval establishes the initial abstract state
    REQUIRE(C.eval(state));
    REQUIRE(state["x"].getLower().value == 10);
    REQUIRE(state["x"].getUpper().value == 20);

    // Now narrow should detect no further tightening possible
    AnalyzedValue oldX = state["x"];
    bool narrowed = C.narrow(state);

    // Since [10,20] is already the narrowest possible, narrow should return false
    REQUIRE_FALSE(narrowed);
    REQUIRE(state["x"].getLower().value == 10);
    REQUIRE(state["x"].getUpper().value == 20);
    REQUIRE(state["x"] == oldX);
}

TEST_CASE("Narrow tightens lower bound from -infinity", "[constraints][narrow]") {
    AbstractState state;

    // Start with a lower-unbounded interval (-inf, 100]
    AnalyzedValue x;
    AnalyzedValue::Bound low;
    low.type = AnalyzedValue::Bound::Type::MinusInfinity;

    AnalyzedValue::Bound up;
    up.type = AnalyzedValue::Bound::Type::Constant;
    up.value = 100;

    x.setAsInterval(low, up, 1);
    state["x"] = x;

    // Intersection with [10, 20] should give [10, 20]
    AnalyzedValue::Bound ten;
    ten.type = AnalyzedValue::Bound::Type::Constant;
    ten.value = 10;

    AnalyzedValue::Bound twenty;
    twenty.type = AnalyzedValue::Bound::Type::Constant;
    twenty.value = 20;

    IntersectionConstraint C("x", "x", ten, twenty);

    REQUIRE(C.eval(state));
    REQUIRE(state["x"].getLower().value == 10);
    REQUIRE(state["x"].getUpper().value == 20);

    bool narrowed = C.narrow(state);
    REQUIRE_FALSE(narrowed); // Already at fixed point
}

TEST_CASE("Narrow tightens upper bound to +infinity", "[constraints][narrow]") {
    AbstractState state;

    // Start with an upper-unbounded interval [0, +inf)
    AnalyzedValue x;
    AnalyzedValue::Bound low;
    low.type = AnalyzedValue::Bound::Type::Constant;
    low.value = 0;

    AnalyzedValue::Bound up;
    up.type = AnalyzedValue::Bound::Type::PlusInfinity;

    x.setAsInterval(low, up, 1);
    state["x"] = x;

    // Intersection with [10, 20] should give [10, 20]
    AnalyzedValue::Bound ten;
    ten.type = AnalyzedValue::Bound::Type::Constant;
    ten.value = 10;

    AnalyzedValue::Bound twenty;
    twenty.type = AnalyzedValue::Bound::Type::Constant;
    twenty.value = 20;

    IntersectionConstraint C("x", "x", ten, twenty);

    REQUIRE(C.eval(state));
    REQUIRE(state["x"].getLower().value == 10);
    REQUIRE(state["x"].getUpper().value == 20);

    bool narrowed = C.narrow(state);
    REQUIRE_FALSE(narrowed);
}

TEST_CASE("Narrow gradually tightens interval", "[constraints][narrow]") {
    AbstractState state;

    // Start with wide interval [-100, 100]
    AnalyzedValue x;
    AnalyzedValue::Bound low;
    low.type = AnalyzedValue::Bound::Type::Constant;
    low.value = -100;

    AnalyzedValue::Bound up;
    up.type = AnalyzedValue::Bound::Type::Constant;
    up.value = 100;

    x.setAsInterval(low, up, 1);
    state["x"] = x;

    // Create a chain of intersection constraints that progressively narrow
    AnalyzedValue::Bound neg50;
    neg50.type = AnalyzedValue::Bound::Type::Constant;
    neg50.value = -50;

    AnalyzedValue::Bound pos50;
    pos50.type = AnalyzedValue::Bound::Type::Constant;
    pos50.value = 50;

    IntersectionConstraint C1("x", "x", neg50, pos50);

    // First narrowing: [-100,100] -> [-50,50]
    REQUIRE(C1.eval(state));
    REQUIRE(state["x"].getLower().value == -50);
    REQUIRE(state["x"].getUpper().value == 50);

    // Narrow should detect change and return true
    bool narrowed = C1.narrow(state);
    REQUIRE(narrowed); // We tightened bounds

    // Second narrowing: [-50,50] -> [0,25]
    AnalyzedValue::Bound zero;
    zero.type = AnalyzedValue::Bound::Type::Constant;
    zero.value = 0;

    AnalyzedValue::Bound twentyFive;
    twentyFive.type = AnalyzedValue::Bound::Type::Constant;
    twentyFive.value = 25;

    IntersectionConstraint C2("x", "x", zero, twentyFive);
    REQUIRE(C2.eval(state));
    REQUIRE(state["x"].getLower().value == 0);
    REQUIRE(state["x"].getUpper().value == 25);

    narrowed = C2.narrow(state);
    REQUIRE(narrowed); // Another tightening

    // Third narrowing: [0,25] -> [10,20]
    AnalyzedValue::Bound ten;
    ten.type = AnalyzedValue::Bound::Type::Constant;
    ten.value = 10;

    AnalyzedValue::Bound twenty;
    twenty.type = AnalyzedValue::Bound::Type::Constant;
    twenty.value = 20;

    IntersectionConstraint C3("x", "x", ten, twenty);
    REQUIRE(C3.eval(state));
    REQUIRE(state["x"].getLower().value == 10);
    REQUIRE(state["x"].getUpper().value == 20);

    narrowed = C3.narrow(state);
    REQUIRE(narrowed); // Final tightening

    // One more narrow should return false (fixed point reached)
    narrowed = C3.narrow(state);
    REQUIRE_FALSE(narrowed);
}

TEST_CASE("Narrow with no changes returns false", "[constraints][narrow]") {
    AbstractState state;

    // Start with a precise singleton [5,5]
    AnalyzedValue x;
    AnalyzedValue::Bound five;
    five.type = AnalyzedValue::Bound::Type::Constant;
    five.value = 5;

    x.setAsInterval(five, five, 1);
    state["x"] = x;

    // Intersection that doesn't change the value [5,5]
    AnalyzedValue::Bound zero;
    zero.type = AnalyzedValue::Bound::Type::Constant;
    zero.value = 0;

    AnalyzedValue::Bound ten;
    ten.type = AnalyzedValue::Bound::Type::Constant;
    ten.value = 10;

    IntersectionConstraint C("x", "x", zero, ten);

    REQUIRE(C.eval(state));
    REQUIRE(state["x"].getLower().value == 5);
    REQUIRE(state["x"].getUpper().value == 5);

    // Narrow should detect no change and return false
    bool narrowed = C.narrow(state);
    REQUIRE_FALSE(narrowed);
}

TEST_CASE("Narrow with Phi constraint convergence", "[constraints][narrow]") {
    AbstractState state;

    // Initialize two variables
    AnalyzedValue x;
    AnalyzedValue::Bound low;
    low.type = AnalyzedValue::Bound::Type::Constant;
    low.value = 0;

    AnalyzedValue::Bound up;
    up.type = AnalyzedValue::Bound::Type::Constant;
    up.value = 100;
    x.setAsInterval(low, up, 1);
    state["x"] = x;
    state["y"] = x; // y starts same as x

    // Phi constraint: z = phi(x, y)
    PhiConstraint phi("z", {"x", "y"});
    REQUIRE(phi.eval(state));

    // Initially z = x ∪ y = [0,100]
    REQUIRE(state["z"].getLower().value == 0);
    REQUIRE(state["z"].getUpper().value == 100);

    // Now narrow z using a constraint that should tighten it
    AnalyzedValue::Bound ten;
    ten.type = AnalyzedValue::Bound::Type::Constant;
    ten.value = 10;

    AnalyzedValue::Bound fifty;
    fifty.type = AnalyzedValue::Bound::Type::Constant;
    fifty.value = 50;

    IntersectionConstraint intersect("z", "z", ten, fifty);
    REQUIRE(intersect.eval(state));
    REQUIRE(state["z"].getLower().value == 10);
    REQUIRE(state["z"].getUpper().value == 50);

    // Narrow should detect the change
    bool narrowed = intersect.narrow(state);
    REQUIRE(narrowed);

    // Second narrow should return false (fixed point)
    narrowed = intersect.narrow(state);
    REQUIRE_FALSE(narrowed);
}

TEST_CASE("Narrow with widening and narrowing sequence", "[constraints][narrow]") {
    AbstractState state;

    // Start with wide interval [-1000, 1000]
    AnalyzedValue x;
    AnalyzedValue::Bound low;
    low.type = AnalyzedValue::Bound::Type::Constant;
    low.value = -1000;

    AnalyzedValue::Bound up;
    up.type = AnalyzedValue::Bound::Type::Constant;
    up.value = 1000;

    x.setAsInterval(low, up, 1);
    state["x"] = x;

    // Simulate a loop that first widens then narrows
    // First iteration: add constant 500 (widens upper bound)
    AnalyzedValue y = state["x"];
    y.addConstant(500);
    state["x"] = y;
    REQUIRE(state["x"].getUpper().value == 1000); // Already had 1000

    // Add constant 1500 (widens to +infinity)
    y = state["x"];
    y.addConstant(1500);
    state["x"] = y;
    REQUIRE(state["x"].getUpper().type == AnalyzedValue::Bound::Type::PlusInfinity);

    // Now narrow back down using intersection
    AnalyzedValue::Bound neg100;
    neg100.type = AnalyzedValue::Bound::Type::Constant;
    neg100.value = -100;

    AnalyzedValue::Bound pos100;
    pos100.type = AnalyzedValue::Bound::Type::Constant;
    pos100.value = 100;

    IntersectionConstraint C("x", "x", neg100, pos100);
    REQUIRE(C.eval(state));
    REQUIRE(state["x"].getLower().value == -100);
    REQUIRE(state["x"].getUpper().value == 100);

    // Narrow should detect the significant change
    bool narrowed = C.narrow(state);
    REQUIRE(narrowed);

    // No further change
    narrowed = C.narrow(state);
    REQUIRE_FALSE(narrowed);
}
