#include "Graph.h"

VariableVertex* ConstraintGraph::getOrCreateVar(const std::string& name) {
    auto it = data_vertices.find(name);
    if (it == data_vertices.end()) {
        auto v = std::make_shared<VariableVertex>();
        v->name = name;
        data_vertices[name] = v;
        return v.get();
    }
    return it->second.get();
}

void ConstraintGraph::addConstraint(std::shared_ptr<Constraint> c) {
    // 1. Register the variable definition
    VariableVertex* def_var = getOrCreateVar(c->def);
    def_var->defined_by = c;

    // 2. Register the variable usage
    for (const UseEdge& edge : c->get_uses()) {
        VariableVertex* use_var = getOrCreateVar(edge.target_variable);
        use_var->used_by.push_back({c, edge.type});
    }

    operation_vertices.push_back(std::move(c));
}

std::vector<std::vector<std::shared_ptr<Constraint>>> 
ConstraintGraph::getTopologicalSCCs() const {
    int timer = 0;
    std::unordered_map<std::shared_ptr<Constraint>, int> index;
    std::unordered_map<std::shared_ptr<Constraint>, int> lowlink;
    std::unordered_map<std::shared_ptr<Constraint>, bool> on_stack;
    std::stack<std::shared_ptr<Constraint>> st;
    
    std::vector<std::vector<std::shared_ptr<Constraint>>> sccs;

    auto tarjan = [&](auto& self, std::shared_ptr<Constraint> u) -> void {
        index[u] = lowlink[u] = timer++;
        st.push(u);
        on_stack[u] = true;

        // Find constraints that use the variable defined by 'u'
        auto it = data_vertices.find(u->def);
        if (it != data_vertices.end()) {
            for (const auto& edge : it->second->used_by) {
                std::shared_ptr<Constraint> v = edge.target.lock();
                
                if (!v) continue;

                if (index.find(v) == index.end()) { 
                    self(self, v);
                    lowlink[u] = std::min(lowlink[u], lowlink[v]);
                } else if (on_stack[v]) { 
                    lowlink[u] = std::min(lowlink[u], index[v]);
                }
            }
        }

        if (lowlink[u] == index[u]) {
            std::vector<std::shared_ptr<Constraint>> current_scc;
            std::shared_ptr<Constraint> w;
            do {
                w = st.top();
                st.pop();
                on_stack[w] = false;
                current_scc.push_back(w);
            } while (w != u);
            
            sccs.push_back(current_scc);
        }
    };

    for (const auto& c_ptr : operation_vertices) {
        if (index.find(c_ptr) == index.end()) {
            tarjan(tarjan, c_ptr);
        }
    }

    std::reverse(sccs.begin(), sccs.end());
    return sccs;
}
