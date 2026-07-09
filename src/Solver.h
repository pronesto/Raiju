#ifndef SOLVER_H
#define SOLVER_H

#include "AbstractValue.h"
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
            growthAnalysis();
            futureResolution();
            narrowingAnalysis();
        }
    
        void growthAnalysis()
        {
            bool changed_evaluating = true;

            while(changed_evaluating){
                changed_evaluating = false;
                for(auto& constraint: constraints){
                    if(constraint->eval(this->state)) changed_evaluating = true;
                }
            }
        }

        void narrowingAnalysis()
        {
            bool changed_narrowing = true;
            
            while(changed_narrowing) {
                changed_narrowing = false;

                for(auto& constraint: constraints)
                {
                    AbstractState stateBeforeEval = this->state;
                    bool is_eval = constraint->eval(this->state);
                    
                    if(is_eval)
                    {
                        AbstractState stateAfterEval = this->state;
                        if(constraint->narrow(stateBeforeEval, stateAfterEval)) changed_narrowing = true;
                    }
                }
            }
        }

    private:

        void futureResolution(){
            for(auto& [var_name, val]: state){
                if(var.hasFutureBound())
                {
                    std::string target = var.getFutureTarget();
                    AnalyzedValue targetValue = state[target];
                    AnalyzedValue concrete = targetValue.addOffset(val.getFutureOffset());
                }
            }
        }
};

#endif