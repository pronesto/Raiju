#include "BoolValue.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("BoolValue - addConstant test", "[lattice]") {
  BoolValue val;

  SECTION("Starts as an empty set (Bottom element)") {
    REQUIRE(val.getKind() == BoolValue::Kind::Set);
  }
  
  SECTION("addConstant add elements to values and keep BoolValue as a Set") {
    std::vector<bool> vals = {true};
    std::set<bool> expectedResult = {true};
    val.addConstant(vals);

    REQUIRE(val.getKind() == BoolValue::Kind::Set);
    REQUIRE(val.getValues() == expectedResult);

    vals = {false};
    expectedResult = {true, false};
    val.addConstant(vals);
    REQUIRE(val.getKind() == BoolValue::Kind::Set);
    REQUIRE(val.getValues() == expectedResult);
  }

  SECTION("addConstant cannot increase the set size up to 2") {
    std::vector<bool> vals = {true, false, true, false, true, false};
    val.addConstant(vals);

    REQUIRE(val.getKind() == BoolValue::Kind::Set);
    REQUIRE(val.getValues().size() == 2);
  }
}

TEST_CASE("BoolValue - join test", "[lattice]") {
  SECTION("join merge elements from both sets") {
    BoolValue val, other;
    std::vector<bool> vals = {true};
    std::set<bool> expectedResult = {true,false};
    val.addConstant(vals);

    vals = {true, false};
    other.addConstant(vals);

    val.join(other);

    REQUIRE(val.getKind() == BoolValue::Kind::Set);
    REQUIRE(val.getValues() == expectedResult);
  }

  SECTION("join doesn't add new elements") {
    BoolValue val, other;
    std::vector<bool> vals = {false};
    std::set<bool> expectedResult = {false};
    val.addConstant(vals);

    vals = {false};
    other.addConstant(vals);

    val.join(other);

    REQUIRE(val.getKind() == BoolValue::Kind::Set);
    REQUIRE(val.getValues() == expectedResult);
  }

  SECTION("join makes empty set incorporate other set") {
    BoolValue val, other;
    
    std::vector<bool> vals = {false};
    other.addConstant(vals);

    val.join(other);

    REQUIRE(val.getKind() == BoolValue::Kind::Set);
    REQUIRE(val == other);
  }

  SECTION("join doesn't modify other set") {
    BoolValue val, other;
    std::vector<bool> vals = {true};
    std::set<bool> expectedResult = {true, false};
    val.addConstant(vals);
    
    vals = {false};
    other.addConstant(vals);

    val.join(other);

    REQUIRE(val.getKind() == BoolValue::Kind::Set);
    REQUIRE(val != other);
    REQUIRE(val.getValues() == expectedResult);
  }
}

