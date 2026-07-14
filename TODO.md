I think we need some major refactoring. I think we should do the following:

    1. Create an AbstractValue for each variable right in the beginning and add that to the State table. Make the constructor private, so that you cannot create more of them, e.g., via AddConstraint

    2. Only modify an AbstractValue through its API, e.g., addConstant, decreaseLowerLimit, increaseUpperLimit

    3. Add a counter to the AbstractValue object, that keeps track of how many times you are calling these methods.

    4. Add a method called cropLowerLimit and one called cropUpperLimit, to allow narrowing to work.


What I'd do is to keep all this state inside AbstractValue, and use the following effectfull API:

- `bool addConstant(C)` // Increases the counter if it returns true.
- `bool decreaseLower(C)` // Increases the counter if it returns true.
- `bool increaseUpper(C)` // Increases the counter if it returns true.

Notice that this would be super imperative... but I think that's ok. We would still need methods that don't change the counter, to be able to do narrowing, e.g.:

- `bool narrowLower(C) // Returns true if it changed, but don't touch the counter.`
- `bool narrowUpper(C) // Returns true if it changed, but don't touch the counter.`

Then, eval would be, in general, like so:

```cpp
bool AddConstraint::eval(AbstractState& A) {
    AnalyzedValue old_val = A[def];    
    const AnalyzedValue &lhs = A[op1];
    const AnalyzedValue &rhs = A[op2];
    // bool changed = old_val.change
    return changed;
}
```