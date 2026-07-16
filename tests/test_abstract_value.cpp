#include "AbstractValue.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("AbstractValue - Finite Set behavior below capacity", "[lattice]") {
  // Instantiate with a capacity of N = 3
  AbstractValue<3> val;

  SECTION("Starts as an empty set (Bottom element)") {
    REQUIRE(val.getKind() == AbstractValue<3>::Kind::Set);
  }

  SECTION("Stays a set when adding unique constants up to N") {
    std::vector<int> vals = {3,5};
    val.addConstant(vals);

    REQUIRE(val.getKind() == AbstractValue<3>::Kind::Set);

    // Adding a duplicate shouldn't change the size or representation
    vals = {5};
    val.addConstant(vals);
    REQUIRE(val.getKind() == AbstractValue<3>::Kind::Set);
  }
}

TEST_CASE("AbstractValue - Collapse to Strided Interval at capacity N",
          "[lattice]") {
  AbstractValue<3> val; // N = 3

  SECTION("Collapses to StridedInterval when exceeding N constants") {
    std::vector<int> vals = {3, 5, 11};
    val.addConstant(vals);

    // Currently at size 3 (exactly N). Should still be a set.
    REQUIRE(val.getKind() == AbstractValue<3>::Kind::Set);

    // Adding the 4th distinct element pushes it past N = 3
    vals = {13};
    val.addConstant(vals);

    // It must collapse into a StridedInterval representation
    REQUIRE(val.getKind() == AbstractValue<3>::Kind::StridedInterval);
  }
}

TEST_CASE("AbstractValue - Lattice Join Operations", "[lattice]") {
  SECTION("Join of two small sets stays a set if combined size <= N") {
    AbstractValue<4> lhs;
    AbstractValue<4> rhs;

    std::vector<int> vals = {2};
    lhs.addConstant(vals);
    vals = {4};
    rhs.addConstant(vals);

    lhs.join(rhs);

    // Total unique elements = 2, which is <= 4
    REQUIRE(lhs.getKind() == AbstractValue<4>::Kind::Set);
  }

  SECTION("Join of two sets collapses if combined size > N") {
    AbstractValue<2> lhs;
    AbstractValue<2> rhs;

    std::vector<int> vals = {10, 20};
    lhs.addConstant(vals); // lhs has 2 elements

    vals = {30};
    rhs.addConstant(
        vals); // joining will create a set of 3 elements, exceeding N=2

    lhs.join(rhs);
    REQUIRE(lhs.getKind() == AbstractValue<2>::Kind::StridedInterval);
  }
}

TEST_CASE("AbstractValue - Bound Equality Operators", "[lattice][equality]") {
  using Bound = AbstractValue<3>::Bound;

  SECTION("Constant bounds match based on exact value") {
    Bound b1{Bound::Type::Constant, 42};
    Bound b2{Bound::Type::Constant, 42};
    Bound b3{Bound::Type::Constant, 99};

    REQUIRE(b1 == b2);
    REQUIRE(b1 != b3);
  }

  SECTION("Infinities match by type regardless of junk value data") {
    Bound inf1{Bound::Type::PlusInfinity, 0};
    Bound inf2{Bound::Type::PlusInfinity, 1234}; // Different underlying value
    Bound minf{Bound::Type::MinusInfinity, 0};

    REQUIRE(inf1 == inf2);
    REQUIRE(inf1 != minf);
  }

  SECTION("Different bound types are never equal") {
    Bound c{Bound::Type::Constant, 0};
    Bound inf{Bound::Type::PlusInfinity, 0};

    REQUIRE(c != inf);
  }
}

TEST_CASE("AbstractValue - Lattice Structural Equality",
          "[lattice][equality]") {
  SECTION("Empty sets (Bottom elements) are equal") {
    AbstractValue<3> val1;
    AbstractValue<3> val2;

    REQUIRE(val1 == val2);
  }

  SECTION("Sets with identical elements are equal") {
    AbstractValue<3> val1;
    AbstractValue<3> val2;

    std::vector<int> vals = {10, 20};
    val1.addConstant(vals);

    vals = {20, 10};
    val2.addConstant(
        vals); // Order of insertion shouldn't matter as internal vector is sorted

    REQUIRE(val1 == val2);
  }

  SECTION("Sets with different elements are unequal") {
    AbstractValue<3> val1;
    AbstractValue<3> val2;

    std::vector<int> vals = {10};
    val1.addConstant(vals);
    vals = {20};
    val2.addConstant(vals);

    REQUIRE(val1 != val2);
  }

  SECTION("Set and StridedInterval are never equal") {
    AbstractValue<2> val1; // N = 2
    AbstractValue<2> val2;

    // val1 stays a set of 2 elements
    
    std::vector<int> vals = {2, 4};
    val1.addConstant(vals);

    // val2 collapses into an interval (2, 6, 2) because it exceeds capacity N=2
    vals = {2,4,6};
    val2.addConstant(vals);

    REQUIRE(val1 != val2);
  }

  SECTION("StridedInterval elements with matching configurations are equal") {
    AbstractValue<2> val1;
    AbstractValue<2> val2;

    // Force both to collapse into the exact same interval bounds and strides
    std::vector<int> vals = {5,10,15};
    val1.addConstant(vals);
    val2.addConstant(vals);

    REQUIRE(val1 == val2);
  }

  SECTION(
      "StridedInterval elements with differing strides or bounds are unequal") {
    AbstractValue<2> val1;
    AbstractValue<2> val2;

    // Both collapse, but will have different structural dimensions
    // (bounds/strides)
    std::vector<int> vals = {2,4,6};
    val1.addConstant(vals); // (2, 6, 2)
    vals = {2,5,8};
    val2.addConstant(vals); // (2, 8, 3)

    REQUIRE(val1 != val2);
  }
}

#include "AbstractValue.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("AbstractValue - Bound Relational Operators",
          "[lattice][comparison]") {
  using Bound = AbstractValue<3>::Bound;

  SECTION("Constant vs Constant comparisons") {
    Bound b_low{Bound::Type::Constant, 10};
    Bound b_high{Bound::Type::Constant, 20};
    Bound b_same{Bound::Type::Constant, 10};

    REQUIRE(b_low < b_high);
    REQUIRE(b_low <= b_high);
    REQUIRE(b_low <= b_same);
    REQUIRE_FALSE(b_high < b_low);

    REQUIRE(b_high > b_low);
    REQUIRE(b_high >= b_low);
    REQUIRE(b_same >= b_low);
  }

  SECTION("Infinities dominate comparisons correctly") {
    Bound minf{Bound::Type::MinusInfinity, -999};
    Bound pinf{Bound::Type::PlusInfinity, 999};
    Bound c{Bound::Type::Constant, 0};

    // Minus infinity checks
    REQUIRE(minf < c);
    REQUIRE(minf <= c);
    REQUIRE(minf < pinf);
    REQUIRE_FALSE(minf > c);

    // Plus infinity checks
    REQUIRE(pinf > c);
    REQUIRE(pinf >= c);
    REQUIRE(pinf > minf);
    REQUIRE_FALSE(pinf < c);

    // Strict identical type boundaries are not strictly less/greater
    REQUIRE_FALSE(minf < minf);
    REQUIRE_FALSE(pinf > pinf);
    REQUIRE(minf <= minf);
    REQUIRE(pinf >= pinf);
  }
}

TEST_CASE("AbstractValue - AbstractValue Over-Approximate Comparisons",
          "[lattice][comparison]") {

  SECTION("Empty sets (bottom elements) evaluate to false for safety") {
    AbstractValue<3> empty1;
    AbstractValue<3> empty2;
    AbstractValue<3> populated;
    std::vector<int> vals = {5};
    populated.addConstant(vals);

    // Comparisons involving empty elements must return false under sound
    // over-approximation
    REQUIRE_FALSE(empty1 < populated);
    REQUIRE_FALSE(populated < empty1);
    REQUIRE_FALSE(empty1 < empty2);
    REQUIRE_FALSE(empty1 <= empty2);
  }

  SECTION("Set vs Set sound comparisons") {
    AbstractValue<3> set_low; // {1, 2}
    std::vector<int> vals = {1,2};
    set_low.addConstant(vals);

    AbstractValue<3> set_high; // {10, 11}
    vals = {10,11};
    set_high.addConstant(vals);

    AbstractValue<3> set_overlap; // {2, 5}
    vals = {2,5};
    set_overlap.addConstant(vals);

    // Completely disjoint and ordered
    REQUIRE(set_low < set_high);
    REQUIRE(set_low <= set_high);
    REQUIRE(set_high > set_low);
    REQUIRE(set_high >= set_low);

    // Overlapping paths cannot be soundly ordered
    // Fails because max(set_low) is not < min(set_overlap) (2 is not < 2)
    REQUIRE_FALSE(set_low < set_overlap);
    // Passes because max(set_low) <= min(set_overlap) (2 <= 2)
    REQUIRE(set_low <= set_overlap);
  }

  SECTION("Set vs StridedInterval mixed comparisons") {
    AbstractValue<3> set; // {1, 2, 3}
    std::vector<int> vals = {1, 2, 3};
    set.addConstant(vals);

    AbstractValue<3> interval; // Collapses into interval [10, 20] with stride 5
    vals = {10, 15, 20, 25};
    interval.addConstant(vals); // Forces collapse since N=3

    REQUIRE(set < interval);
    REQUIRE(set <= interval);
    REQUIRE_FALSE(interval < set);
  }

  SECTION("StridedInterval vs Infinities") {
    AbstractValue<3> interval; // [10, 20] collapsed
    std::vector<int> vals = {10, 15, 20, 25};
    interval.addConstant(vals);

    AbstractValue<3> infinite_top;
    infinite_top.setAsInterval(
        {AbstractValue<3>::Bound::Type::Constant, 100},
        {AbstractValue<3>::Bound::Type::PlusInfinity, 0});

    AbstractValue<3> infinite_bottom;
    infinite_bottom.setAsInterval(
        {AbstractValue<3>::Bound::Type::MinusInfinity, 0},
        {AbstractValue<3>::Bound::Type::Constant, -5});

    REQUIRE(interval < infinite_top);
    REQUIRE(infinite_bottom < interval);

    // Cannot cleanly order an upper infinite interval against a regular high
    // window
    REQUIRE_FALSE(infinite_top < interval);
    REQUIRE_FALSE(interval < infinite_bottom);
  }
}
