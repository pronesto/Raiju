#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <stack>
#include <algorithm>
#include "Constraint.h"

// Usamos weak_ptr para arestas para evitar ciclos de posse e memory leaks
struct VariableVertex {
    std::string name;
    std::weak_ptr<Constraint> defined_by; 

    struct OutEdge {
        std::weak_ptr<Constraint> target; // Quem usa essa variável?
        EdgeType type;
    };
    std::vector<OutEdge> used_by; 
};

class ConstraintGraph {
private:
    std::vector<std::shared_ptr<Constraint>> operation_vertices;
    std::unordered_map<std::string, std::shared_ptr<VariableVertex>> data_vertices;

    VariableVertex* getOrCreateVar(const std::string& name) {
        auto it = data_vertices.find(name);
        if (it == data_vertices.end()) {
            auto v = std::make_shared<VariableVertex>();
            v->name = name;
            data_vertices[name] = v;
            return v.get();
        }
        return it->second.get();
    }

public:
    void addConstraint(std::shared_ptr<Constraint> c) {
        // 1. Registra a definição da variável
        VariableVertex* def_var = getOrCreateVar(c->def);
        def_var->defined_by = c;

        // 2. Registra o uso da variável
        for (const UseEdge& edge : c->get_uses()) {
            VariableVertex* use_var = getOrCreateVar(edge.target_variable);
            
            // "c" é um shared_ptr, podemos passá-lo diretamente para o construtor da OutEdge
            // que converte implicitamente para weak_ptr
            use_var->used_by.push_back({c, edge.type});
        }

        operation_vertices.push_back(std::move(c));
    }

    std::vector<std::vector<std::shared_ptr<Constraint>>> getTopologicalSCCs() const {
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

            // Busca as restrições que usam a variável definida por 'u'
            auto it = data_vertices.find(u->def);
            if (it != data_vertices.end()) {
                for (const auto& edge : it->second->used_by) {
                    // Recupera o shared_ptr do weak_ptr
                    std::shared_ptr<Constraint> v = edge.target.lock();
                    
                    if (!v) continue; // Constraint inválida ou destruída

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
};