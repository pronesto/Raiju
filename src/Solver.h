#ifndef SOLVER_H
#define SOLVER_H

#include "Constraint.h"
#include <vector>
#include <memory>

class Solver{
    AbstractState& state;
    std::vector<std::shared_ptr<Constraint>> constraints;

    public:
        explicit Solver(AbstractState& state): state(state){}

        void addConstraint(std::shared_ptr<Constraint> constraint){
            constraints.push_back(constraint);
        }

        void solve(){
            bool changed_evaluating = true;

            while(changed_evaluating){
                changed_evaluating = false;
                for(auto& constraint: constraints){
                    if(constraint->eval(this->state)) changed_evaluating = true;
                }
            }
        

            resolveFeatures();

            bool changed_narrowing = true;
            
            while(changed_narrowing) {
                changed_narrowing = false;

                for(auto& constraint: constraints)
                {
                    if(constraint->narrow(this->state)) changed_narrowing = true;
                }
            }
        }

    private:
        void resolveFeatures(){
            for(auto& [var_name, val]: state){
                val.concretize();
            }
        }

}

#endif