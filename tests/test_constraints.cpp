#include "Constraint.h"
#include <catch2/catch_test_macros.hpp>

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
    std::vector<int> vals = {42};
    expected.addConstant(vals);
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
    std::vector<int> vals = {3,5};
    expected.addConstant(vals);
    REQUIRE(state["x0"] == expected);
  }

  SECTION("Phi constraint forces collapse into StridedInterval if combined "
          "capacity > N") {
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
    REQUIRE(state["phi_overflow"].getKind() ==
            AnalyzedValue::Kind::StridedInterval);
  }
}

#include "Constraint.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Constraints - AddConstraint Pairwise Sets", "[constraints][add]") {
  AbstractState state;

  // v1 = {2, 3}
  InitializationConstraint init_v1_a("v1", 2);
  init_v1_a.eval(state);
  // Since addConstant or join isn't directly exposed in Constraint,
  // let's simulate a set by using a Phi style layout or a mock update.
  // For test isolation, we'll manually push data into the map or use a custom
  // tool. But since state is an unordered_map, we can populate it directly in
  // the test!

  // v1 = {2,3}
  AnalyzedValue v1;
  std::vector<int> vals = {2,3};
  v1.addConstant(vals);
  state["v1"] = v1;

  // v2 = {10, 20}
  vals = {10,20};
  AnalyzedValue v2;
  v2.addConstant(vals);
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
    std::vector<int> vals = {12, 13, 22, 23};
    expected.addConstant(vals);

    REQUIRE(state["v0"] == expected);
  }
}

TEST_CASE("Constraints - AddConstraint Overflow and Interval Math",
          "[constraints][add]") {
  AbstractState state;

  AnalyzedValue v1;
  std::vector<int> vals = {1,2,3};
  v1.addConstant(vals);
  state["v1"] = v1;

  AnalyzedValue v2;
  vals = {10,20};
  v2.addConstant(vals);
  state["v2"] = v2;

  AddConstraint add_v0("v0", "v1", "v2");

  SECTION("Addition widens after the finite-set capacity is exceeded") {

    REQUIRE(add_v0.eval(state));

    const auto &result = state["v0"];

    REQUIRE(result.getKind() == AnalyzedValue::Kind::StridedInterval);

    REQUIRE(result.getLower().getConstant() == 11);

    REQUIRE(result.getUpper().getConstant() == 23);

    REQUIRE(result.getStride() == 1);
  }
}

TEST_CASE("Intersection with constant upper bound",
          "[constraints][intersect]") {

  AbstractState state;

  AnalyzedValue v;
  std::vector<int> vals = {1,5,10};
  v.addConstant(vals);

  state["x"] = v;

  Bound minusInf = Bound::minusInfinity();

  Bound five = Bound::constant(5);

  IntersectionConstraint C("y", "x", minusInf, five);

  REQUIRE(C.eval(state));

  REQUIRE(state["y"].getKind() == AnalyzedValue::Kind::Set);

  REQUIRE(state["y"].getValues()==std::set<int>{1,5});
}

TEST_CASE("Intersection narrows interval", "[constraints][intersect]") {

  AbstractState state;

  AnalyzedValue x;

  Bound low = Bound::constant(0);

  Bound up = Bound::constant(100);

  x.setAsInterval(low, up, 1);

  state["x"] = x;

  Bound ten = Bound::constant(10);

  Bound twenty = Bound::constant(20);

  IntersectionConstraint C("y", "x", ten, twenty);

  REQUIRE(C.eval(state));

  REQUIRE(state["y"].getLower().getConstant() == 10);
  REQUIRE(state["y"].getUpper().getConstant() == 20);
}

TEST_CASE("Growth phase ignores futures", "[constraints][intersect]") {

  AbstractState state;

  AnalyzedValue x;
  std::vector<int> vals = {1,5,10};
  x.addConstant(vals);

  state["x"] = x;

  IntersectionConstraint::Future F{"y", -1};

  Bound minusInf = Bound::minusInfinity();

  IntersectionConstraint C("z", "x", minusInf, F);

  REQUIRE(C.eval(state));

  REQUIRE(state["z"] == state["x"]);
}

TEST_CASE("Narrowing recovers from MinusInfinity lower bound",
          "[constraints][narrow]") {
  AbstractState state;

  // Set up operand x = [0, 50]
  AnalyzedValue x;
  Bound zero = Bound::constant(0);
  Bound fifty = Bound::constant(50);
  x.setAsInterval(zero, fifty, 1);
  state["x"] = x;

  // Set up destination y old state = [-Infinity, 100]
  AnalyzedValue y_old;
  Bound minusInf = Bound::minusInfinity();
  Bound hundred = Bound::constant(100);
  y_old.setAsInterval(minusInf, hundred, 1);
  state["y"] = y_old;

  // Constraint: y = x intersection [10, 20] -> eval(state) will yield [10, 20]
  Bound ten = Bound::constant(10);
  Bound twenty = Bound::constant(20);
  IntersectionConstraint C("y", "x", ten, twenty);

  // Call narrow: should return true because the lower bound shrinks from -Inf
  // to 10
  REQUIRE(C.narrow(state));

  // Guard 1 updates 'lo' to 10, but because of the 'else if' ladder,
  // 'hi' retains oldY's upper bound (100) instead of falling through to eY's
  // upper bound (20)
  REQUIRE(state["y"].getLower().getConstant() == 10);
  REQUIRE(state["y"].getUpper().getConstant() == 20);
}

TEST_CASE("Narrowing tightens a finite upper bound", "[constraints][narrow]") {
  AbstractState state;

  // Set up operand x = [0, 100]
  AnalyzedValue x;
  Bound zero = Bound::constant(0);
  Bound hundred = Bound::constant(100);
  x.setAsInterval(zero, hundred, 1);
  state["x"] = x;

  // Set up destination y old state = [0, 100]
  AnalyzedValue y_old = x;
  state["y"] = y_old;

  // Constraint: y = x intersection [0, 50] -> eval(state) yields [0, 50]
  // Lower bounds match (0 == 0), but upper bound shrinks (50 < 100)
  Bound fifty = Bound::constant(50);
  IntersectionConstraint C("y", "x", zero, fifty);

  // Should narrow successfully
  REQUIRE(C.narrow(state));

  // Lower bound stays 0, upper bound is narrowed to 50
  REQUIRE(state["y"].getLower().getConstant() == 0);
  REQUIRE(state["y"].getUpper().getConstant() == 50);
}

TEST_CASE("Narrowing reaches a fixed point and returns false",
          "[constraints][narrow]") {
  AbstractState state;

  // Set up operand x = [10, 20]
  AnalyzedValue x;
  Bound ten = Bound::constant(10);
  Bound twenty = Bound::constant(20);
  x.setAsInterval(ten, twenty, 1);
  state["x"] = x;

  // Set up destination y old state = [10, 20]
  state["y"] = x;

  // Constraint: y = x intersection [10, 20] -> eval(state) yields [10, 20]
  IntersectionConstraint C("y", "x", ten, twenty);

  // No shrinking happens; should return false to signal a fixed point
  REQUIRE_FALSE(C.narrow(state));

  // Ensure state values are untouched
  REQUIRE(state["y"].getLower().getConstant() == 10);
  REQUIRE(state["y"].getUpper().getConstant() == 20);
}

TEST_CASE("Resolve future lower bound",
          "[constraints][intersect][future]") {

  AbstractState state;

  // x = [10, 20]
  AnalyzedValue x;
  Bound ten = Bound::constant(10);

  Bound twenty = Bound::constant(20);

  x.setAsInterval(ten, twenty);
  state["x"] = x;

  Bound plusInf = Bound::plusInfinity();

  IntersectionConstraint::Future future{"x", 3};

  IntersectionConstraint C(
      "y",
      "z",
      future,
      plusInf);

  auto resolved = C.resolveFutures(state);

  AbstractState dummy;

  AnalyzedValue z;
  z.setAsInterval(
      Bound::constant(0),
      Bound::constant(30));

  dummy["z"] = z;

  REQUIRE(resolved.eval(dummy));

  AnalyzedValue expected;
  expected.setAsInterval(
      Bound::constant(13),
      Bound::constant(30));

  REQUIRE(dummy["y"] == expected);
}

TEST_CASE("Resolve future upper bound",
          "[constraints][intersect][future]") {

  AbstractState state;

  // x = [10, 20]
  AnalyzedValue x;
  Bound ten = Bound::constant(10);

  Bound twenty = Bound::constant(20);

  x.setAsInterval(ten, twenty);
  state["x"] = x;

  Bound minusInf = Bound::minusInfinity();

  IntersectionConstraint::Future future{"x", -2};

  IntersectionConstraint C(
      "y",
      "z",
      minusInf,
      future);

  auto resolved = C.resolveFutures(state);

  AbstractState dummy;

  AnalyzedValue z;
  z.setAsInterval(
      Bound::constant(0),
      Bound::constant(30));

  dummy["z"] = z;

  REQUIRE(resolved.eval(dummy));

  AnalyzedValue expected;
  expected.setAsInterval(
      Bound::constant(0),
      Bound::constant(18));

  REQUIRE(dummy["y"] == expected);
}


TEST_CASE("Constraints - MultiplyConstraint Overflow",
          "[constraints][mul]") {
    AbstractState state;

    AnalyzedValue a;
    std::vector<int> vals = {1,2,3};
    a.addConstant(vals);
    state["a"] = a;

    vals = {10,20};
    AnalyzedValue b;
    b.addConstant(vals);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");

    SECTION("Multiplication widens after the finite-set capacity is exceeded") {

        REQUIRE(multiply.eval(state));

        const auto &result = state["c"];

        REQUIRE(result.getKind() == AnalyzedValue::Kind::StridedInterval);

        REQUIRE(result.getLower().getConstant() == 10);
        REQUIRE(result.getUpper().getConstant() == 60);
        
        REQUIRE(result.getStride() == 10);
    }
}

TEST_CASE("Constraints - MultiplyConstraint Fixed Point",
          "[constraints][mul]") {
  AbstractState state;

  AnalyzedValue a;
  std::vector<int> vals = {6};
  a.addConstant(vals);
  state["a"] = a;

  vals = {7};
  AnalyzedValue b;
  b.addConstant(vals);
  state["b"] = b;

  MultiplyConstraint multiply("c", "a", "b");

  REQUIRE(multiply.eval(state));

  SECTION("Second evaluation returns false") {
    REQUIRE_FALSE(multiply.eval(state));
  }
}

TEST_CASE("Constraints - MultiplyConstraint With Zero",
          "[constraints][mul]") {
  AbstractState state;

  AnalyzedValue a;
  std::vector<int> vals = {0};
  a.addConstant(vals);
  state["a"] = a;

  vals = {5};
  AnalyzedValue b;
  b.addConstant(vals);
  state["b"] = b;

  MultiplyConstraint multiply("c", "a", "b");

  REQUIRE(multiply.eval(state));

  vals = {0};
  AnalyzedValue expected;
  expected.addConstant(vals);

  REQUIRE(state["c"] == expected);
}

TEST_CASE("Constraints - MultiplyConstraint Negative Values",
          "[constraints][mul]") {
  AbstractState state;

  AnalyzedValue a;
  std::vector<int> vals = {-2};
  a.addConstant(vals);
  state["a"] = a;

  vals = {4};
  AnalyzedValue b;
  b.addConstant(vals);
  state["b"] = b;

  MultiplyConstraint multiply("c", "a", "b");

  REQUIRE(multiply.eval(state));

  vals = {-8};
  AnalyzedValue expected;
  expected.addConstant(vals);

  REQUIRE(state["c"] == expected);
}

TEST_CASE("Constraints - LinearConstraint Set Behavior", "[constraints][linear]") {
  AbstractState state;

  // v1 = {1, 2, 3}
  AnalyzedValue v1;
  std::vector<int> vals = {1,2,3};
  v1.addConstant(vals);
  state["v1"] = v1;

  // v0 = 2 * v1 + 5
  // Results (2*1)+5=7, (2*2)+5=9, (2*3)+5=11
  LinearConstraint lin_v0("v0", "v1", 2, 5);

  SECTION("Correctly computes linear transformation on sets") {
    bool changed = lin_v0.eval(state);

    REQUIRE(changed == true);
    REQUIRE(state["v0"].getKind() == AnalyzedValue::Kind::Set);

    AnalyzedValue expected;
    std::vector<int> vals = {7,9,11};
    expected.addConstant(vals);

    REQUIRE(state["v0"] == expected);
  }

  SECTION("Second evaluation returns false (Fixed Point)") {
    lin_v0.eval(state);
    REQUIRE_FALSE(lin_v0.eval(state));
  }
}

TEST_CASE("Constraints - LinearConstraint Interval Behavior", "[constraints][linear]") {
  AbstractState state;

  // v1 = [10, 20]
  AnalyzedValue v1;
  Bound low = Bound::constant(10);
  Bound up = Bound::constant(20);
  v1.setAsInterval(low, up, 1);
  state["v1"] = v1;

  // v0 = 3 * v1 - 2
  // New min: 3*10 - 2 = 28
  // New max: 3*20 - 2 = 58
  LinearConstraint lin_v0("v0", "v1", 3, -2);

  SECTION("Correctly computes linear transformation on intervals") {
    REQUIRE(lin_v0.eval(state));

    const auto& res = state["v0"];
    REQUIRE(res.getKind() == AnalyzedValue::Kind::StridedInterval);
    REQUIRE(res.getLower().getConstant() == 28);
    REQUIRE(res.getUpper().getConstant() == 58);
  }
}

TEST_CASE("Constraints - LinearConstraint Negative Multiplier", "[constraints][linear]") {
  AbstractState state;

  // v1 = [0, 10]
  AnalyzedValue v1;
  v1.setAsInterval(Bound::constant(0), 
                   Bound::constant(10), 1);
  state["v1"] = v1;

  // v0 = -1 * v1 + 5
  // k1 = -1*0 + 5 = 5
  // ku = -1*10 + 5 = -5
  // min = -5, max = 5
  LinearConstraint lin_v0("v0", "v1", -1, 5);

  SECTION("Handles negative multiplier in intervals") {
    REQUIRE(lin_v0.eval(state));

    const auto& res = state["v0"];
    
    REQUIRE(res.getLower().getConstant() == -5);
    REQUIRE(res.getUpper().getConstant() == 5);
  }
}

TEST_CASE("Constraints - LinearConstraint Identity", "[constraints][linear]") {
  AbstractState state;

  AnalyzedValue v1;
  std::vector<int> vals = {42};
  v1.addConstant(vals);
  state["v1"] = v1;

  LinearConstraint identity("v0", "v1", 1, 0);

  SECTION("Preserves value under identity operation") {
    identity.eval(state);
    REQUIRE(state["v0"].getValues() == std::set<int>{42});
  }
}

TEST_CASE("Constraints - MultiplyConstraint Infinite Bounds", "[constraints][mul]") {
  AbstractState state;

  SECTION("Multiplication: [0, 0] * [2, 3]") {
    // [0,0] * [2,3] -> [0,0]
    AnalyzedValue a;
    a.setAsInterval(Bound::constant(0), Bound::constant(0), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(2), Bound::constant(3), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().getConstant() == 0);
    REQUIRE(result.getUpper().getConstant() == 0);
  }

  SECTION("Multiplication: [0, 0] * [-inf, +inf]") {
    // [0,0] * [-inf, +inf] ->   [0,0]
    AnalyzedValue a;
    a.setAsInterval(Bound::constant(0), Bound::constant(0), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::minusInfinity(), Bound::plusInfinity(), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().getConstant() == 0);
    REQUIRE(result.getUpper().getConstant() == 0);
  }

  SECTION("Multiplication: [-inf, +inf] * [2, 4]") {
    // [-inf, +inf] * [x2,y2] -> [-inf, +inf]
    AnalyzedValue a;
    a.setAsInterval(Bound::minusInfinity(), Bound::plusInfinity(), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(2), Bound::constant(4), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().isMinusInfinity());
    REQUIRE(result.getUpper().isPlusInfinity());
  }

  SECTION("Multiplication: [2,4] * [-inf, +inf]") {
    // [x1,y1] * [-inf, +inf] -> [-inf, +inf]
    AnalyzedValue a;
    a.setAsInterval(Bound::constant(2), Bound::constant(4), 1);
    state["a"] = a;
    
    AnalyzedValue b;
    b.setAsInterval(Bound::minusInfinity(), Bound::plusInfinity(), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().isMinusInfinity());
    REQUIRE(result.getUpper().isPlusInfinity());
  }

  SECTION("Multiplication: [-inf, 2] * [1, 30]") {
    // [-inf, y1] * [x2,y2] ->   [-inf, y1*y2]
    AnalyzedValue a;
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(2), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(1), Bound::constant(30), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().isMinusInfinity());
    REQUIRE(result.getUpper().getConstant() == 60);
  }

  SECTION("Multiplication: [-inf, 2] * [0, 30]") {
    // [-inf, y1] * [0,y2] ->   [-inf, y1*y2]
    AnalyzedValue a;
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(2), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(0), Bound::constant(30), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().isMinusInfinity());
    REQUIRE(result.getUpper().getConstant() == 60);
  }

  SECTION("Multiplication: [-inf, -2] * [0, 30]") {
    // [-inf, -y1] * [0,y2] ->   [-inf, y1*y2]
    AnalyzedValue a;
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(-2), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(0), Bound::constant(30), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().isMinusInfinity());
    REQUIRE(result.getUpper().getConstant() == 0);
  }

  SECTION("Multiplication: [-inf, 0] * [0, 30]") {
    // [-inf, 0] * [0,y2] ->   [-inf, y1*y2]
    AnalyzedValue a;
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(0), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(0), Bound::constant(30), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().isMinusInfinity());
    REQUIRE(result.getUpper().getConstant() == 0);
  }

  SECTION("Multiplication: [-inf, 2] * [-1, 30]") {
    // [-inf, y1] * [-x2,y2] ->  [-inf, +inf]
    AnalyzedValue a;
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(2), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(-1), Bound::constant(30), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().isMinusInfinity());
    REQUIRE(result.getUpper().isPlusInfinity());
  }

  SECTION("Multiplication: [-inf, 2] * [-1, 0]") {
    // [-inf, y1] * [-x2,0] ->   [y1*-x2, +inf]
    AnalyzedValue a;
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(2), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(-1), Bound::constant(0), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().getConstant() == -2);
    REQUIRE(result.getUpper().isPlusInfinity());
  }

  SECTION("Multiplication: [-inf, -2] * [-1, 0]") {
    // [-inf, y1] * [-x2,0] ->   [y1*-x2, +inf]
    AnalyzedValue a;
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(-2), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(-1), Bound::constant(0), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().getConstant() == 0);
    REQUIRE(result.getUpper().isPlusInfinity());
  }

  SECTION("Multiplication: [-inf, 2] * [-1, 0]") {
    // [-inf, y1] * [-x2,0] ->   [y1*-x2, +inf]
    AnalyzedValue a;
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(0), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(-1), Bound::constant(0), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().getConstant() == 0);
    REQUIRE(result.getUpper().isPlusInfinity());
  }
  

  SECTION("Multiplication: [-inf, 2] * [1, 30]") {
    // [-inf, y1] * [x2,y2] ->   [-inf, y1*y2]
    AnalyzedValue a;
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(2), 1);
    state["a"] = a;

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(1), Bound::constant(30), 1);
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b");
    multiply.eval(state);

    const auto &result = state["c"];
    REQUIRE(result.getLower().isMinusInfinity());
    REQUIRE(result.getUpper().getConstant() == 60);
  }

  SECTION("[-inf, y1] * [-x2, -y2] -> [y1 * -x2, +inf]") {
    // [-inf, 2] * [-10, -5] -> [2 * -10, +inf] -> [-20, +inf]
    AnalyzedValue a; a.setAsInterval(Bound::minusInfinity(), Bound::constant(2), 1);
    AnalyzedValue b; b.setAsInterval(Bound::constant(-10), Bound::constant(-5), 1);
    state["a"] = a; state["b"] = b;
    MultiplyConstraint m("c", "a", "b"); m.eval(state);
    REQUIRE(state["c"].getLower().getConstant() == -20);
    REQUIRE(state["c"].getUpper().isPlusInfinity());
  }

  SECTION("[-inf, -y1] * [x2, y2] -> [-inf, -y1 * x2]") {
    // [-inf, -2] * [3, 5] -> [-inf, -6]
    AnalyzedValue a; 
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(-2), 1);
    
    AnalyzedValue b; 
    b.setAsInterval(Bound::constant(3), Bound::constant(5), 1);
    
    state["a"] = a; 
    state["b"] = b;
    
    MultiplyConstraint multiply("c", "a", "b"); 
    multiply.eval(state);

    REQUIRE(state["c"].getLower().isMinusInfinity());
    REQUIRE(state["c"].getUpper().getConstant() == -6);
  }

  SECTION("[-inf, -y1] * [0, y2] -> [-inf, 0]") {
    AnalyzedValue a; 
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(-2), 1);

    AnalyzedValue b; 
    b.setAsInterval(Bound::constant(0), Bound::constant(5), 1);
    
    state["a"] = a; 
    state["b"] = b;
    MultiplyConstraint multiply("c", "a", "b"); 
    multiply.eval(state);
    
    REQUIRE(state["c"].getLower().isMinusInfinity());
    REQUIRE(state["c"].getUpper().getConstant() == 0);
  }

  SECTION("[x1, +inf] * [x2, y2] -> [x1 * x2, +inf]") {
    // [2, +inf] * [3, 4] -> [6, +inf]
    AnalyzedValue a; 
    a.setAsInterval(Bound::constant(2), Bound::plusInfinity(), 1);

    AnalyzedValue b; 
    b.setAsInterval(Bound::constant(3), Bound::constant(4), 1);

    state["a"] = a; 
    state["b"] = b;
    
    MultiplyConstraint multiply("c", "a", "b"); 
    multiply.eval(state);

    REQUIRE(state["c"].getLower().getConstant() == 6);
    REQUIRE(state["c"].getUpper().isPlusInfinity());
  }

  SECTION("[x1, +inf] * [0, y2] -> [0, +inf]") {
    AnalyzedValue a; 
    a.setAsInterval(Bound::constant(2), Bound::plusInfinity(), 1);

    AnalyzedValue b; 
    b.setAsInterval(Bound::constant(0), Bound::constant(4), 1);

    state["a"] = a; 
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b"); 
    multiply.eval(state);
    
    REQUIRE(state["c"].getLower().getConstant() == 0);
    REQUIRE(state["c"].getUpper().isPlusInfinity());
  }

  SECTION("[x1, +inf] * [-x2, 0] -> [-inf, 0]") {
    AnalyzedValue a; 
    a.setAsInterval(Bound::constant(2), Bound::plusInfinity(), 1);
    AnalyzedValue b; 
    b.setAsInterval(Bound::constant(-5), Bound::constant(0), 1);
    
    state["a"] = a; 
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b"); 
    multiply.eval(state);
    
    REQUIRE(state["c"].getLower().isMinusInfinity());
    REQUIRE(state["c"].getUpper().getConstant() == 0);
  }

  SECTION("[-x1, +inf] * [-x2, -y2] -> [-inf, x1 * x2]") {
    // [-2, +inf] * [-5, -3] -> [-inf, 10]
    AnalyzedValue a; 
    a.setAsInterval(Bound::constant(-2), Bound::plusInfinity(), 1);

    AnalyzedValue b; 
    b.setAsInterval(Bound::constant(-5), Bound::constant(-3), 1);

    state["a"] = a; 
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b"); 
    multiply.eval(state);
    
    REQUIRE(state["c"].getLower().isMinusInfinity());
    REQUIRE(state["c"].getUpper().getConstant() == 10);
  }

  SECTION("[-x1, +inf] * [0, y2] -> [-inf, x1 * y2]") {
    // [-2, +inf] * [0, 3] -> [-inf, -6]
    AnalyzedValue a; 
    a.setAsInterval(Bound::constant(-2), Bound::plusInfinity(), 1);

    AnalyzedValue b; 
    b.setAsInterval(Bound::constant(0), Bound::constant(3), 1);

    state["a"] = a; 
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b"); 
    multiply.eval(state);
    
    REQUIRE(state["c"].getLower().getConstant() == -6);
    REQUIRE(state["c"].getUpper().isPlusInfinity());
  }

  SECTION("[-x1, +inf] * [-x2, 0] -> [-inf, x1 * x2]") {
    // [-2, +inf] * [-5, 0] -> [-inf, 10]
    AnalyzedValue a; 
    a.setAsInterval(Bound::constant(-2), Bound::plusInfinity(), 1);

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(-5), Bound::constant(0), 1);

    state["a"] = a; 
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b"); 
    multiply.eval(state);
    
    REQUIRE(state["c"].getLower().isMinusInfinity());
    REQUIRE(state["c"].getUpper().getConstant() == 10);
  }

  SECTION("[-x1, 0] * [-x2, +inf] -> [-inf, x1 * x2]") {
    // [-2, +inf] * [-5, 0] -> [-inf, 10]
    AnalyzedValue a; 
    a.setAsInterval(Bound::constant(-5), Bound::constant(0), 1);

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(-2), Bound::plusInfinity(), 1);

    state["a"] = a; 
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b"); 
    multiply.eval(state);
    
    REQUIRE(state["c"].getLower().isMinusInfinity());
    REQUIRE(state["c"].getUpper().getConstant() == 10);
  }

  SECTION("[-inf, +inf] * [0, 0] -> [0, 0]") {
    // [-2, +inf] * [-5, 0] -> [-inf, 10]
    AnalyzedValue a; 
    a.setAsInterval(Bound::minusInfinity(), Bound::plusInfinity(), 1);

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(0), Bound::constant(0), 1);

    state["a"] = a; 
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b"); 
    multiply.eval(state);
    
    REQUIRE(state["c"].getLower().getConstant() == 0);
    REQUIRE(state["c"].getUpper().getConstant() == 0);
  }

  SECTION("[-inf, 0] * [0, +inf] -> [-inf, 0]") {
    // [-2, +inf] * [-5, 0] -> [-inf, 10]
    AnalyzedValue a; 
    a.setAsInterval(Bound::minusInfinity(), Bound::constant(0), 1);

    AnalyzedValue b;
    b.setAsInterval(Bound::constant(0), Bound::plusInfinity(), 1);

    state["a"] = a; 
    state["b"] = b;

    MultiplyConstraint multiply("c", "a", "b"); 
    multiply.eval(state);
    
    REQUIRE(state["c"].getLower().isMinusInfinity());
    REQUIRE(state["c"].getUpper().getConstant() == 0);
  }
}
