#include <iostream>
#include "IntValue.h"

int main() {
    std::cout << "Lattice Static Analyzer Engine Initialized." << std::endl;
    
    // Quick smoke test of your template domain
    IntValue<4> val;
    std::vector<int> vals = {42};
    val.addConstant(vals);
    
    std::cout << "Success! Lattice state kind: " 
              << (val.getKind() == IntValue<4>::Kind::Set ?
                  "Set" : "StridedInterval")
              << std::endl;
              
    return 0;
}
