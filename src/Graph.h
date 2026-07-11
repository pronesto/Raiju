#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <stack>
#include <algorithm>
#include "Constraint.h"

/**
 * @brief Represents a variable vertex in the constraint graph.
 * 
 * Tracks which constraint defines the variable and which constraints use it.
 */
struct VariableVertex {
    std::string name;                              ///< Name of the variable
    std::weak_ptr<Constraint> defined_by;          ///< Constraint that defines this variable

    /**
     * @brief Edge from a variable to a constraint that uses it.
     */
    struct OutEdge {
        std::weak_ptr<Constraint> target;          ///< Constraint that uses this variable
        EdgeType type;                              ///< Type of the edge (use relationship)
    };
    std::vector<OutEdge> used_by;                  ///< Constraints that use this variable
};

/**
 * @brief Represents a constraint graph for dependency analysis.
 * 
 * This class manages a graph where vertices are variables and constraints,
 * and edges represent definition and usage relationships.
 */
class ConstraintGraph {
private:
    std::vector<std::shared_ptr<Constraint>> operation_vertices;
    std::unordered_map<std::string, std::shared_ptr<VariableVertex>> data_vertices;

    /**
     * @brief Gets or creates a variable vertex by name.
     * @param name The variable name.
     * @return Pointer to the VariableVertex.
     */
    VariableVertex* getOrCreateVar(const std::string& name);

public:
    /**
     * @brief Adds a constraint to the graph.
     * @param c Shared pointer to the constraint to add.
     */
    void addConstraint(std::shared_ptr<Constraint> c);

    /**
     * @brief Computes the strongly connected components (SCCs) in topological order.
     * @return Vector of SCCs, each SCC is a vector of Constraints.
     */
    std::vector<std::vector<std::shared_ptr<Constraint>>> getTopologicalSCCs() const;
};
