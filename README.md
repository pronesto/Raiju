# Lattice Static Analyzer

A minimal C++ prototype of a static analysis tool that implements a **Finite-Set / Strided-Interval Abstract Domain**. This lattice combines the precision of tracking exact constant sets with the efficiency and scalability of strided interval analysis.

---

## The Abstract Domain Explained

Standard interval analysis struggles with precision when unrelated constants meet in code execution paths. For instance, given a conditional merge yielding $\{3, 5, 11\}$, a classic interval domain infers the range $[3, 11]$, introducing false positives (like $4$ or $6$). 

[To solve this, this implementation uses a hybrid domain bounded by a compile-time constant $N$:

* **Finite Sets ($\le N$):** Stores exact sets of reachable constants $\{c_0, c_1, \dots, c_k\}$ where $k < N$. Arithmetic operations on these elements are completely exact.
* **Strided Intervals ($> N$):** When the size of a set exceeds $N$, the domain gracefully collapses into a compact tuple representation $(l, u, s)$ representing the mathematical sequence $\{x \mid l \le x \le u \land x \equiv l \pmod s\}$. 

This allows the compiler optimization passes (such as constant propagation or array bounds elimination) to preserve precise value info for small constant sets without facing exponential state space explosions.

---

## Getting Started

### Prerequisites

* A C++17 compatible compiler (GCC 8+, Clang 7+, or MSVC 2019+)
* CMake (version 3.14 or higher)
* Git (to automatically pull the Catch2 testing framework)

---

## Building the Project

We use CMake to orchestrate the build process. Execute the following commands in your terminal from the root of the project:

```bash
# 1. Create and move into a build directory
mkdir build && cd build

# 2. Configure the project and fetch dependencies (e.g., Catch2)
cmake ..

# 3. Compile both the analyzer and the test suite
cmake --build .

```

---

## Running the Executables

### Run the Main Analyzer

After a successful build, you can execute the primary application:

```bash
./lattice_analyzer

```

### Run the Unit Test Suite

The project integrates with `ctest` to seamlessly execute unit tests and verify the mathematical correctness of your lattice joins and transfer functions:

```bash
ctest --output-on-failure

```
