#include <catch2/catch_test_macros.hpp>
#include "AbstractValue.h"

TEST_CASE("AbstractValue - Finite Set behavior below capacity", "[lattice]") {
    // Instantiate with a capacity of N = 3
    AbstractValue<3> val;

    SECTION("Starts as an empty set (Bottom element)") {
        REQUIRE(val.getKind() == AbstractValue<3>::Kind::Set);
    }

    SECTION("Stays a set when adding unique constants up to N") {
        val.addConstant(3);
        val.addConstant(5);
        
        REQUIRE(val.getKind() == AbstractValue<3>::Kind::Set);
        
        // Adding a duplicate shouldn't change the size or representation
        val.addConstant(5);
        REQUIRE(val.getKind() == AbstractValue<3>::Kind::Set);
    }
}

TEST_CASE("AbstractValue - Collapse to Strided Interval at capacity N", "[lattice]") {
    AbstractValue<3> val; // N = 3

    SECTION("Collapses to StridedInterval when exceeding N constants") {
        val.addConstant(3);
        val.addConstant(5);
        val.addConstant(11);
        
        // Currently at size 3 (exactly N). Should still be a set.
        REQUIRE(val.getKind() == AbstractValue<3>::Kind::Set);

        // Adding the 4th distinct element pushes it past N = 3
        val.addConstant(13);
        
        // It must collapse into a StridedInterval representation
        REQUIRE(val.getKind() == AbstractValue<3>::Kind::StridedInterval);
    }
}

TEST_CASE("AbstractValue - Lattice Join Operations", "[lattice]") {
    SECTION("Join of two small sets stays a set if combined size <= N") {
        AbstractValue<4> lhs;
        AbstractValue<4> rhs;

        lhs.addConstant(2);
        rhs.addConstant(4);

        lhs.join(rhs);
        
        // Total unique elements = 2, which is <= 4
        REQUIRE(lhs.getKind() == AbstractValue<4>::Kind::Set);
    }

    SECTION("Join of two sets collapses if combined size > N") {
        AbstractValue<2> lhs;
        AbstractValue<2> rhs;

        lhs.addConstant(10);
        lhs.addConstant(20); // lhs has 2 elements

        rhs.addConstant(30); // joining will create a set of 3 elements, exceeding N=2

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

TEST_CASE("AbstractValue - Lattice Structural Equality", "[lattice][equality]") {
    SECTION("Empty sets (Bottom elements) are equal") {
        AbstractValue<3> val1;
        AbstractValue<3> val2;

        REQUIRE(val1 == val2);
    }

    SECTION("Sets with identical elements are equal") {
        AbstractValue<3> val1;
        AbstractValue<3> val2;

        val1.addConstant(10);
        val1.addConstant(20);

        val2.addConstant(20);
        val2.addConstant(10); // Order of insertion shouldn't matter as internal vector is sorted

        REQUIRE(val1 == val2);
    }

    SECTION("Sets with different elements are unequal") {
        AbstractValue<3> val1;
        AbstractValue<3> val2;

        val1.addConstant(10);
        val2.addConstant(20);

        REQUIRE(val1 != val2);
    }

    SECTION("Set and StridedInterval are never equal") {
        AbstractValue<2> val1; // N = 2
        AbstractValue<2> val2;

        // val1 stays a set of 2 elements
        val1.addConstant(2);
        val1.addConstant(4);

        // val2 collapses into an interval (2, 6, 2) because it exceeds capacity N=2
        val2.addConstant(2);
        val2.addConstant(4);
        val2.addConstant(6);

        REQUIRE(val1 != val2);
    }

    SECTION("StridedInterval elements with matching configurations are equal") {
        AbstractValue<2> val1;
        AbstractValue<2> val2;

        // Force both to collapse into the exact same interval bounds and strides
        val1.addConstant(5); val1.addConstant(10); val1.addConstant(15);
        val2.addConstant(5); val2.addConstant(10); val2.addConstant(15);

        REQUIRE(val1 == val2);
    }

    SECTION("StridedInterval elements with differing strides or bounds are unequal") {
        AbstractValue<2> val1;
        AbstractValue<2> val2;

        // Both collapse, but will have different structural dimensions (bounds/strides)
        val1.addConstant(2); val1.addConstant(4); val1.addConstant(6); // (2, 6, 2)
        val2.addConstant(2); val2.addConstant(5); val2.addConstant(8); // (2, 8, 3)

        REQUIRE(val1 != val2);
    }
}
