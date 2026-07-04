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
