#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <stack>
#include <algorithm>
#include "Constraint.h"

struct VariableVertex {
    std::string name;
    Constraint* defined_by = nullptr; 
    

    struct OutEdge {
        Constraint* target;
        EdgeType type;
    };
    std::vector<OutEdge> used_by; 
};

class ConstraintGraph {
private:

    std::vector<std::unique_ptr<Constraint>> operation_vertices;
    std::unordered_map<std::string, std::unique_ptr<VariableVertex>> data_vertices;

    VariableVertex* getOrCreateVar(const std::string& name) {
        if (data_vertices.find(name) == data_vertices.end()) {
            auto v = std::make_unique<VariableVertex>();
            v->name = name;
            data_vertices[name] = std::move(v);
        }
        return data_vertices[name].get();
    }

public:
    /**
     * @brief Adiciona uma restrição e constrói as arestas do Grafo.
     */
    void addConstraint(std::unique_ptr<Constraint> c) {
        Constraint* C = c.get();
        
        VariableVertex* def_var = getOrCreateVar(C->def);
        def_var->defined_by = C;

        for (const UseEdge& edge : C->get_uses()) {
            VariableVertex* use_var = getOrCreateVar(edge.target_variable);
            use_var->used_by.push_back({C, edge.type});
        }

        operation_vertices.push_back(std::move(c));
    }

    /**
     * @brief Calcula os SCCs e retorna em Ordem Topológica.
     * Usando o Algoritmo de Tarjan adaptado para projetar o grafo C -> v -> C.
     */
    std::vector<std::vector<Constraint*>> getTopologicalSCCs() const {

        int timer = 0;
        std::unordered_map<Constraint*, int> index;
        std::unordered_map<Constraint*, int> lowlink;
        std::unordered_map<Constraint*, bool> on_stack;
        std::stack<Constraint*> st;
        
        std::vector<std::vector<Constraint*>> sccs;

        auto tarjan = [&](auto& self, Constraint* u) -> void {
            index[u] = lowlink[u] = timer++;
            st.push(u);
            on_stack[u] = true;

            const std::string& def_var_name = u->def;
            auto it = data_vertices.find(def_var_name);
            
            if (it != data_vertices.end()) {
                for (const auto& edge : it->second->used_by) {
                    Constraint* v = edge.target;
                    
                    if (index.find(v) == index.end()) { 
                        self(self, v);
                        lowlink[u] = std::min(lowlink[u], lowlink[v]);
                    } else if (on_stack[v]) { 
                        lowlink[u] = std::min(lowlink[u], index[v]);
                    }
                }
            }

            if (lowlink[u] == index[u]) {
                std::vector<Constraint*> current_scc;
                Constraint* w;
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
            Constraint* C = c_ptr.get();
            if (index.find(C) == index.end()) {
                tarjan(tarjan, C);
            }
        }

        std::reverse(sccs.begin(), sccs.end());

        return sccs;
    }
};
