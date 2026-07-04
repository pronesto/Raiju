#include <iostream>
#include "AbstractValue.h"

int main() {
    std::cout << "Lattice Static Analyzer Engine Initialized." << std::endl;
    
    // Quick smoke test of your template domain
    AbstractValue<4> val;
    val.addConstant(42);
    
    std::cout << "Success! Lattice state kind: " 
              << (val.getKind() == AbstractValue<4>::Kind::Set ?
                  "Set" : "StridedInterval")
              << std::endl;
              
    return 0;
}
