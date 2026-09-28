# SANKHYA — COMPLETE COMPETITOR INTELLIGENCE REPORT
**Target Repository:** `ALN-web/sih26` (Local path: `c:\Users\Hamada Salim G-Trd\sih26119\sih26`)  
**SIH 2026 Problem Statement:** **SIH26119** — *"Indigenous GPU-Accelerated Optimization Solver (Sovereign Alternative to CPLEX / Xpress)"*  
**Issuing Organization:** Mangalore Refinery and Petrochemicals Limited (MRPL)  
**Author / Audience:** Competitive Intelligence & Strategic Architecture Team for SIH26119  
**Audit Date:** September 2026

---

## 1. Executive Summary

SANKHYA is an in-memory mathematical optimization solver written in **C++20** that implements Linear Programming (LP), Mixed-Integer Linear Programming (MILP), and Convex Quadratic Programming (QP).

### Key Findings of this Audit:
1. **The Core Strength:** The developers possess deep theoretical and algorithmic knowledge of classical numerical optimization. They have written a bounded revised dual simplex, a sparse Markowitz LU factorization, an Approximate Minimum Degree (AMD) quotient-graph sparse $\text{LDL}^T$ solver, an 8-reduction presolve/postsolve engine, a Mehrotra predictor-corrector IPM, and an independent rational-arithmetic oracle from first principles with **zero solver library dependencies**.
2. **The Major Vulnerability (The SIH Disconnect):** The problem statement explicitly demands an **"Indigenous GPU-Accelerated Optimization Solver"**. **In SANKHYA, GPU acceleration does not exist.** There is **no CUDA code, no `.cu` kernel, and no GPU offloading** anywhere in the repository. Passing `--gpu` in the CLI literally prints:  
   `"--gpu requested but this build has no CUDA backend compiled in; running on CPU"` (`src/core/solve.cpp:384-386`).
3. **MIP Maturity Gap:** While LP is benchmarked at 78/89 Netlib passes, their Mixed-Integer programming (MILP) is elementary. On MIPLIB 2017, it solves only **9 of 30** easy benchmark instances within 60 seconds. Cutting planes (Gomory, lifted knapsack) exist only at the root node and are disabled by default because they degraded the solve rate.
4. **Our Strategic Advantage:** SANKHYA is an academic, CPU-bound, classical solver that neglected the GPU requirement. By building a **GPU-accelerated, hybrid optimization solver** (with true custom CUDA kernels for parallel matrix-vector operations, batched simplex pivoting, and accelerated first-order ADMM/PDHG) and pairing it with a production-grade Web Dashboard and Refinery Digital Twin, **our team can outperform them on the SIH evaluation rubric.**

---

## 2. What SANKHYA Is (Simple & Technical Explanation)

### Plain English Explanation
Think of a refinery like MRPL deciding every morning how much crude oil of different types (Arab Light, Bonny Light, Murban) to pump into distillation towers to maximize fuel profits while keeping sulphur under 10 ppm and meeting diesel quotas. This problem involves thousands of equations and constraints.  
Usually, Indian companies pay millions of dollars annually to IBM (CPLEX) or FICO (Xpress) for software that calculates these numbers. SANKHYA is an attempt to build that mathematical calculator from scratch in India so refineries do not depend on foreign licenses.

### Technical Definition
Technically, SANKHYA is an in-memory numerical optimization engine with a unified `Model` $\to$ `solve()` $\to$ `Solution` dispatcher (`src/core/solve.cpp`). It compiles into a standalone binary CLI tool (`sankhya-cli`), a C shared library (`libsankhya_core.so` / `sankhya_core.dll`), and a Python ctypes wrapper (`bindings/python/sankhya`).

```
CLAIMED vs IMPLEMENTED vs DEMONSTRATED vs PLANNED:

[CLAIMED IN TITLE / DOCS]
├── "GPU-Accelerated Optimization Solver"   ──────> [NOT IMPLEMENTED / ZERO CODE]
├── "Sovereign Alternative to CPLEX/Xpress" ──────> [CLAIMED / UNREALISTIC SCALE GAP]
├── "Modular for NLP / MINLP"               ──────> [PLANNED / STUB SEAM ONLY]

[IMPLEMENTED IN CODE]
├── Bounded Dual Simplex with Devex         ──────> [IMPLEMENTED & TESTED]
├── Revised Primal Simplex + Composite Ph1 ──────> [IMPLEMENTED & TESTED]
├── Sparse Markowitz LU (Threshold 0.01)    ──────> [IMPLEMENTED & TESTED]
├── Sparse LDLᵀ (AMD Ordering + etree)      ──────> [IMPLEMENTED & TESTED]
├── Mehrotra Interior Point Method (IPM)    ──────> [IMPLEMENTED & TESTED]
├── Restarted PDHG (First-Order CPU)        ──────> [IMPLEMENTED & TESTED]
├── Condat-Vu Convex QP Engine              ──────> [IMPLEMENTED & TESTED]
├── Branch & Bound (Reliability Branching)  ──────> [IMPLEMENTED & TESTED]
├── Presolve (8 Reductions + Postsolve)     ──────> [IMPLEMENTED & TESTED]
└── Independent KKT & Farkas Verifier       ──────> [IMPLEMENTED & TESTED]

[DEMONSTRATED WITH COMMITTED CSV EVIDENCE]
├── 78/89 Netlib Full LP passes             ──────> [DEMONSTRATED IN CSV]
├── 50/50 Agreement with HiGHS on Medium    ──────> [DEMONSTRATED IN CSV]
├── 9/30 MIPLIB 2017 Solved to Gap Target   ──────> [DEMONSTRATED IN CSV]
└── 0/8 Mittelmann Hard Instances Solved    ──────> [DEMONSTRATED FAILED IN CSV]
```

---

## 3. Repository Architecture & Reconnaissance

### Text-Based System Architecture Diagram

```
                               ┌──────────────────────────────────────────────┐
                               │                 ENTRY POINTS                 │
                               │  CLI (apps/sankhya-cli) · C API (src/api)    │
                               │        Python Bindings (ctypes)              │
                               └──────────────────────┬───────────────────────┘
                                                      │
                                                      ▼
                               ┌──────────────────────────────────────────────┐
                               │           I/O & PARSERS (src/io)             │
                               │   MPS Reader (Fixed/Free, Ranges, QuadObj)   │
                               │   LP Format Reader · Solution/JSON Writers   │
                               └──────────────────────┬───────────────────────┘
                                                      │ produces
                                                      ▼
                               ┌──────────────────────────────────────────────┐
                               │              sankhya::Model                  │
                               │ (c, A in CSC/CSR, l, u, row_l, row_u, Q, col_type)
                               └──────────────────────┬───────────────────────┘
                                                      │
                                                      ▼
                               ┌──────────────────────────────────────────────┐
                               │          PRESOLVE (src/presolve)             │
                               │  Singleton rows, doubletons, redundant rows, │
                               │  forcing bounds, empty rows/columns          │
                               └──────────────────────┬───────────────────────┘
                                                      │
                                                      ▼
                       ┌──────────────────────────────────────────────────────────────┐
                       │                   solve() SEAM DISPATCHER                    │
                       │                    (src/core/solve.cpp)                      │
                       └──────┬──────────────┬──────────────┬──────────────┬──────────┘
                              │              │              │              │
              ┌───────────────┘              │              │              └───────────────┐
              ▼                              ▼              ▼                              ▼
      [CONTINUOUS LP]                 [MIXED-INTEGER]   [CONVEX QP]                [MIQP]
 ┌──────────────────────────┐         ┌─────────────┐  ┌─────────────┐      ┌─────────────────┐
 │ Default: Dual Simplex    │         │ Branch &    │  │ Condat-Vu   │      │ Branch & Bound  │
 │ (Devex, bound-flipping)  │         │ Bound (MIP) │  │ Primal-Dual │      │ over QP Nodes   │
 ├──────────────────────────┤         ├─────────────┤  ├─────────────┤      └─────────────────┘
 │ Alt: Primal Simplex      │         │ Reliability │  │ Cholesky    │
 │ (Composite Phase 1)      │         │ Branching   │  │ Convexity   │
 ├──────────────────────────┤         ├─────────────┤  │ Test (LDLᵀ) │
 │ Alt: Mehrotra IPM (LDLᵀ) │         │ Diving      │  └─────────────┘
 ├──────────────────────────┤         │ Heuristic   │
 │ Alt: Restarted PDHG      │         ├─────────────┤
 │ (+ IPM Polish finish)    │         │ Root Cuts   │
 └────────────┬─────────────┘         │ (Off by def)│
              │                       └──────┬──────┘
              ▼                              ▼
 ┌──────────────────────────┐                │
 │ POSTSOLVE (Dual Fixed Pt)│                │
 └────────────┬─────────────┘                │
              │                              │
              └───────────────────────┬──────┘
                                      │
                                      ▼
                       ┌──────────────────────────────┐
                       │    recompute_quality() GUARD │
                       │ KKT Residuals, Dual Infeas., │
                       │ Duality Gap, Integrality     │
                       └──────────────┬───────────────┘
                                      │
                                      ▼
                       ┌──────────────────────────────┐
                       │       sankhya::Solution      │
                       │ Status, Col Values, Duals,   │
                       │ Farkas Ray / Unbounded Cert. │
                       └──────────────┬───────────────┘
                                      │
                                      ▼
                       ┌──────────────────────────────┐
                       │  INDEPENDENT PYTHON VERIFIER │
                       │   (tools/verify_solution.py) │
                       │ Zero C++ code shared, KKT audit
                       └──────────────────────────────┘
```

### Module Breakdown Table

| Directory | Lines of Code | Role in Architecture | Implementation Completeness | Evidence / Critical Files |
| :--- | :--- | :--- | :--- | :--- |
| `include/sankhya/` | ~1,600 | Public API headers, tolerances, options, Model/Solution classes | **100% Complete** | `model.hpp`, `tolerances.hpp`, `sankhya.h` |
| `src/core/` | ~1,500 | `solve()` dispatcher, `recompute_quality()`, status guard, certificates | **100% Complete** | `solve.cpp`, `certificate.cpp` |
| `src/la/` | ~3,700 | Sparse CSC/CSR, Markowitz LU, sparse $\text{LDL}^T$, Ruiz/Pock scaling | **95% Complete** (CPU only) | `lu.cpp`, `ldl.cpp`, `scaling.cpp` |
| `src/presolve/` | ~1,700 | 8 reductions, implied bounds, dual postsolve fixed point | **90% Complete** | `presolve.cpp` |
| `src/simplex/` | ~3,200 | Revised primal & bounded dual simplex, Devex, Harris ratio test | **95% Complete** | `primal_simplex.cpp`, `dual_simplex.cpp` |
| `src/mip/` | ~2,500 | Branch & bound, reliability branching, root cuts (Gomory, knapsack) | **50% Complete** (Weak tree) | `branch_and_bound.cpp`, `cuts.cpp` |
| `src/qp/` | ~600 | Condat-Vu primal-dual convex QP engine, Hessian convexity check | **85% Complete** | `qp_condat_vu.cpp`, `convexity.cpp` |
| `src/ipm/` | ~1,000 | Mehrotra predictor-corrector interior point method | **80% Complete** (No crossover) | `ipm.cpp` |
| `src/pdhg/` | ~750 | Restarted first-order PDHG (CPU only) + IPM polish | **75% Complete** (CPU only) | `pdhg.cpp` |
| `src/gpu/` | **0** | **CUDA kernels, memory transfers, cuBLAS/cuSPARSE bindings** | **0% — NOT IMPLEMENTED** | Directory does not exist on disk |
| `src/api/` | ~500 | C-compatible FFI surface | **100% Complete** | `c_api.cpp` |
| `bindings/python/` | ~400 | Pure ctypes Python wrapper | **90% Complete** | `bindings/python/sankhya/__init__.py` |
| `tools/` | ~1,500 | Independent solution verifier (`verify_solution.py`) | **100% Complete** | `tools/verify_solution.py` |
| `tests/` | ~10,000 | 450+ GoogleTest cases, exact rational oracle, robustness sweeps | **100% Complete** | `tests/CMakeLists.txt`, `tests/oracles/rational_simplex.cpp` |
| `bench/` | ~4,000 | Netlib, MIPLIB, Mittelmann, refinery scale runners, committed CSVs | **100% Complete** | `bench/runners/netlib.py`, `bench/results/` |

---

## 4. Optimization Capabilities Audit

| Optimization Class | Claimed | Implemented | Tested | Demonstrated | Source Evidence |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **LP (Linear)** | Yes | **Yes** | Yes (450+ unit tests) | **Yes** (78/89 Netlib) | `src/simplex/dual_simplex.cpp`, `src/simplex/primal_simplex.cpp` |
| **MILP (Mixed-Integer)** | Yes | **Yes (Weak)** | Yes (unit tests) | **Yes (Partial, 9/30 MIPLIB)** | `src/mip/branch_and_bound.cpp`, `bench/results/miplib-f7ca7e9.csv` |
| **Convex QP** | Yes | **Yes** | Yes (unit tests) | **Yes** (`demo/qp_blend.mps`) | `src/qp/qp_condat_vu.cpp`, `tests/unit/test_qp.cpp` |
| **Non-Convex QP** | No | **Refused** | Yes (negative tests) | **Yes** (Returns Certificate) | `src/qp/convexity.cpp` (LDLᵀ test emits negative pivot) |
| **MIQP (Mixed-Integer QP)** | Yes | **Yes** | Yes (`test_miqp.cpp`) | **Yes** (`demo/miqp_blend.mps`) | `src/core/solve.cpp:459`, `tests/unit/test_miqp.cpp` |
| **NLP (Nonlinear)** | Mentioned | **No** | No | No | `src/core/solve.cpp:480` returns `kNotSolved` |
| **MINLP (Mixed-Integer NL)**| Mentioned | **No** | No | No | `src/core/solve.cpp:480` returns `kNotSolved` |

---

## 5. GPU / CUDA Deep Audit

| Audit Question | SANKHYA Reality | Source Code Verification |
| :--- | :--- | :--- |
| **What runs on GPU?** | **NOTHING.** | Zero `.cu` files in repo. |
| **What remains on CPU?** | **100% of execution.** | Everything runs in C++ on the CPU host. |
| **Is solver GPU accelerated?** | **NO.** | Title claim is unfulfilled. |
| **Which kernels exist?** | **None.** | No CUDA kernels written. |
| **CUDA libraries used?** | None linked in CMake. | `CMakeLists.txt:231-234` has an empty `enable_language(CUDA)` block without target source files. |
| **What happens if user passes `--gpu`?** | Logs a warning and falls back to CPU. | `src/core/solve.cpp:384-386`: `"--gpu requested but this build has no CUDA backend compiled in; running on CPU"` |
| **Is there CPU-GPU memory management?**| None. | No `cudaMalloc`, `cudaMemcpy`, or pinned memory buffers. |
| **Are data structures GPU-friendly?** | No. | Uses nested pointer-like `std::vector<std::vector<...>>` in parts of linear algebra instead of flat CSR buffers suitable for unified memory. |
| **Any GPU benchmark evidence?** | **ZERO.** | All CSVs in `bench/results/` are labeled `machine: Windows-AMD64` or `Linux`, running pure CPU threads. |

---

## 6. Numerical Robustness & Sparse Linear Algebra Audit

### 1. Numerical Tolerances (`include/sankhya/tolerances.hpp`)
- Primal Feasibility: `1e-7`
- Dual Feasibility: `1e-7`
- Integrality Tolerance: `1e-6`
- Markowitz Threshold: `0.01`
- Zero-drop threshold: `1e-11`
- Double precision strictly enforced.

### 2. Matrix Conditioning & Scaling (`src/la/scaling.cpp`)
- **Ruiz equilibration (10 passes)** + **Pock-Chambolle geometric scaling**.
- Successfully scales ill-conditioned models like Netlib `grow7` and `pilot`.

### 3. Sparse Factorization Engine (`src/la/lu.cpp`, `src/la/ldl.cpp`)
- **Sparse LU:** Markowitz threshold pivoting using Suhl & Suhl low-degree search budget (max 4 rows/cols inspected).
- **Product-Form Updates:** Eta file with automatic refactorization on residual error $> 10^{-8}$.
- **Sparse $\text{LDL}^T$:** AMD quotient graph ordering (Amestoy, Davis, Duff 1996) + elimination tree symbolic analysis.

### 4. Independent Verification & Mathematical Proofs
- **Status Guard (`src/core/solve.cpp:244`):** Recomputes $Ax - b$, $c - A^T y$, and duality gaps directly against the **original unscaled model**. If violations exceed $10^{-7}$, the status is downgraded.
- **Certificates (`src/core/certificate.cpp`):**
  - Infeasible: Emits a Farkas vector ($A^T y \ge 0$, $b^T y < 0$).
  - Unbounded: Emits a recession ray ($Ad = 0$, $c^T d > 0$).
  - Non-convex QP: Emits an $\text{LDL}^T$ negative pivot certificate.

---

## 7. Input / Output & API Audit

- **File Formats:** Fixed/Free MPS, Ranges, Bounds, QuadObj QPS, LP format.
- **CLI (`apps/sankhya-cli`):** `sankhya solve`, `info`, `options`, `version`.
- **C API (`include/sankhya/sankhya.h`):** Complete FFI-safe C functions.
- **Python Bindings (`bindings/python/sankhya`):** `ctypes`-based object-oriented wrapper.
- **Original GUI:** **None.** (Addressed by our new interactive Web Dashboard in `web-ui/`).

---

## 8. Benchmark Audit & Real Performance Evidence

### 1. Netlib LP Benchmark (`bench/results/netlib-full-53cbe16.csv`)
- **Pass Rate:** **78 of 89** full-set Netlib instances matched to published optimum within $10^{-6}$ and independently verified.
- **Medium Tier:** 48 of 50 passed.
- **Non-Pass Breakdown:**
  - 7 instances: Agrees with HiGHS to 12 digits; published Netlib table is outlier.
  - 2 instances: Timed out at 120s limit (`dfl001`, `pilot87`).
  - 1 instance: Solved, agrees with HiGHS to $3.0 \times 10^{-7}$, downgraded by internal dual check.
  - 1 instance: Declined to answer due to ratio-test singularity (`maros-r7`).

### 2. MIPLIB 2017 Benchmark (`bench/results/miplib-f7ca7e9.csv`)
- Solved to proven optimum: Only **9 of 30** easy instances within 60s.
- 19 instances feasible only, 2 reached node limit.

### 3. Mittelmann Hard LP Benchmark (`bench/results/mittelmann-f7ca7e9.csv`)
- Solved: **0 of 8** within 300s. All timed out.

### 4. MRPL Refinery Scale Experiment (`bench/results/scale-refinery-f7ca7e9.csv`)
- Monthly (1,068 rows): 1.30s (Simplex), 0.33s (IPM).
- Daily (32,485 rows, 415k nonzeros): 92.5s (IPM), 95.3s (PDHG + Polish). Simplex timed out.
- Hourly (779,640 rows, 9.9M nonzeros): Timed out. PDHG reached $1.14 \times 10^{-6}$ relative error in 133.9s without proof.

---

## 9. Competitor Comparison: SANKHYA vs. World Solvers

| Feature | SANKHYA | HiGHS | SCIP | CBC / Clp | CPLEX / Gurobi / Xpress |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Origin / Sovereignty**| Indigenous India (SIH26) | UK Academic (Open) | German Academic | COIN-OR (Open) | Commercial Proprietary |
| **License** | Apache-2.0 | MIT | Apache-2.0 | EPL-2.0 | Commercial ($$$$) |
| **Code Base** | C++20 (Zero dependencies) | C++11 | C | C++ | C / C++ / Assembly |
| **LP Simplex** | Revised Primal & Dual | Dual Simplex | SoPlex | Clp Simplex | World-class Hyper-sparse |
| **Interior Point (IPM)**| Mehrotra (Normal Eq) | IPM with Crossover | Via external LP | None / External | Parallel Barrier + Crossover |
| **GPU Acceleration** | **None (0%)** | None | Limited Research | None | cuPDLPx / GPU Barrier |
| **MILP Maturity** | Basic B&B (9/30 MIPLIB)| Very Strong | World-leading academic | Moderate | Enterprise (30+ cut types) |
| **Cutting Planes** | Root Gomory & Knapsack | Gomory, MIR, Knapsack | 15+ Cut Families | Gomory, Cgl | Advanced Cut Generators |
| **Convex QP** | Condat-Vu First-Order | Active-set QP | IPM / QP plugin | Clp QP | Convex & Non-convex Barrier |
| **Non-Convex QP** | Refused with certificate| Refused | Spatial B&B (Global) | Local only | Global Bilinear / Spatial B&B|
| **Independent Verifier**| Built-in Python KKT | Separate script | None native | None | Internal validation tools |
| **Industrial UI / Twin**| None originally | None | None | None | Cloud Enterprise Portals |

---

## 10. SANKHYA Maturity Score Matrix

*Scored from 0 (Absent) to 5 (Production & Benchmarked with strong evidence) based strictly on verified code.*

| Module / Dimension | Score (0–5) | Justification & Code Reality |
| :--- | :---: | :--- |
| **Linear Programming (LP)** | **4.5 / 5** | Dual Simplex and IPM are well-engineered; 78/89 Netlib passes independently verified. |
| **Mixed-Integer LP (MILP)** | **2.5 / 5** | Branch & bound works, but cuts are disabled by default; solves only 9/30 MIPLIB instances. |
| **Quadratic Programming (QP)**| **3.5 / 5** | Condat-Vu handles convex QP cleanly; non-convex detection via LDLᵀ is robust. |
| **GPU Acceleration** | **0.0 / 5** | **Completely absent.** No CUDA code, no kernels, `--gpu` flag is a CPU fallback. |
| **Sparse Linear Algebra** | **4.0 / 5** | Markowitz LU, AMD-ordered sparse LDLᵀ, Ruiz/Pock scaling are implemented properly. |
| **Presolve & Postsolve** | **4.0 / 5** | 8 reductions and dual postsolve fixed point reconstructed accurately. |
| **Numerical Safeguards** | **4.5 / 5** | `recompute_quality()`, status guard, and Farkas certificates are implemented well. |
| **Independent Verification** | **5.0 / 5** | `tools/verify_solution.py` is comprehensive and shares zero code with the solver. |
| **API & Integrations** | **3.5 / 5** | C API, Python ctypes wrapper, CLI exist; no GUI originally. |
| **Parallelism / Multi-core** | **1.5 / 5** | Single-threaded by default; OpenMP exists on column loops but shows negligible speedup. |
| **Industrial Readiness** | **3.0 / 5** | Refinery LP models verified, but times out on large-scale Mittelmann models. |
| **TOTAL WEIGHTED MATURITY** | **3.3 / 5** | Strong academic CPU LP solver core; failed GPU deliverable; immature MILP engine. |

---

## 11. What SANKHYA Does Really Well (Top 10 Strengths)

1. **Strict Provenance Discipline:** Zero code copied from CBC, Clp, HiGHS, or GLPK.
2. **Independent Python Verifier (`tools/verify_solution.py`):** 1,197 lines of independent Python KKT verification.
3. **Exact Rational Arithmetic Oracle (`tests/oracles/rational_simplex.cpp`):** Built-in arbitrary-precision fraction solver.
4. **Automated Status Guard (`src/core/solve.cpp:244`):** Disallows false optimism by verifying original model residuals.
5. **Machine-Checkable Mathematical Certificates (`src/core/certificate.cpp`):** Farkas vectors and rays generated automatically.
6. **AMD-Ordered Sparse $\text{LDL}^T$ Factorization (`src/la/ldl.cpp`):** Minimum degree quotient graph ordering reduces factor time significantly.
7. **Dual Simplex Bounded Variable Implementation (`src/simplex/dual_simplex.cpp`):** Forrest-Goldfarb Devex pricing and Koberstein artificial bounds.
8. **Dual Postsolve Fixed-Point Reconstructor (`src/presolve/presolve.cpp`):** Reconstructs true dual multipliers of eliminated rows and columns.
9. **Two-Stage First-Order Polish (`src/core/solve.cpp:180`):** PDHG followed by Mehrotra IPM finish.
10. **Reproducible CSV Evidence (`bench/results/`):** Full commit and timestamp logging on all benchmark data.

---

## 12. SANKHYA's Critical Weaknesses & Vulnerabilities

1. **NO GPU ACCELERATION (Vulnerability #1):** SANKHYA has zero GPU code despite the PS title.
2. **Poor Integer Programming Performance:** Solves only 9 of 30 MIPLIB easy instances within 60s.
3. **Fails on Enterprise-Scale Mittelmann Problems:** 0 of 8 instances solved within 300s.
4. **No Basis Crossover for IPM:** Cannot produce vertex bases or sensitivity shadow prices from IPM.
5. **OpenMP Parallelism Buys Zero Speedup:** Single-threaded iteration loops.
6. **No MIR Cuts or Tree Cuts:** Cuts are only generated at the root node.
7. **No UI or Operator Tooling Originally:** Pure terminal CLI.

---

## 13. Code-Level Bottleneck Analysis

1. **Sparse LU Inversion in Simplex (`src/la/lu.cpp`):** Accumulating eta-vectors cause per-iteration solve time to degrade linearly between refactorizations.
2. **Dense Cliques on Random Graphs (`src/la/ldl.cpp:24`):** Memory complexity scales to $O(n^3)$ on dense expander structures.
3. **Serial Branch-and-Bound Traversal (`src/mip/branch_and_bound.cpp`):** Single-threaded exploration limits combinatorial search speed.

---

## 14. Security, Reliability & Production Readiness

- **Memory Safety:** Modern C++20 RAII patterns; runs clean under ASan and UBSan.
- **Error Handling:** Non-finite values (NaN, Inf) gracefully intercepted and reported as `kNumericalError`.
- **Maturity Level:** **Advanced Research Prototype.** Excellent mathematical rigor, but not production-ready for mission-critical refinery operations due to lack of GPU support and weak integer programming scalability.

---

## 15. The SIH Evaluator Perspective: 25 Hard Evaluation Questions

1. *"Where in your codebase is CUDA, HIP, or OpenCL used?"*
2. *"Why does running `--gpu` in your CLI print that no CUDA backend is compiled in?"*
3. *"Why did you benchmark your scale families exclusively on CPU host memory?"*
4. *"How do you justify claiming GPU readiness when not a single CUDA kernel exists in `src/`?"*
5. *"Why are Gomory and knapsack cuts disabled by default in your MIP solver?"*
6. *"Why does your solver fail 100% (0 of 8) of the Mittelmann benchmark instances within 300 seconds?"*
7. *"Your Interior Point Method does not implement basis crossover. How can an industrial user extract shadow prices from an IPM solve?"*
8. *"Why did your OpenMP parallelization yield 0% speedup on Netlib instances?"*
9. *"Why does your MILP engine fail to solve 21 of the 30 standard MIPLIB easy instances?"*
10. *"How do you handle degenerate cycling in the dual simplex when cost perturbation stalls?"*
11. *"In your refinery blend model, non-linear octane blending constraints are common. Why does SANKHYA only support linear and convex quadratic formulations?"*
12. *"How does a control-room operator in Mangalore inspect schedules without a graphical interface?"*
13. *"If refinery sensors stream throughput telemetry every second, how does your solver support warm-started real-time streaming re-optimization?"*
14. *"Why is there no support for SOCP (Second-Order Cone Programming) or general quadratic constraints?"*
15. *"What is the memory consumption of your branch-and-bound tree on problems with $> 100,000$ nodes?"*
16. *"How do you prevent numerical drift in product-form basis updates on ill-conditioned bases?"*
17. *"How does your presolve handle multi-row aggregation without losing dual feasibility?"*
18. *"Why does your Condat-Vu QP engine require power iterations on every solve rather than computing spectral radius incrementally?"*
19. *"Why does SANKHYA not support MPS files with indicator constraints or SOS1/SOS2 sets?"*
20. *"What is the wall-clock overhead of your postsolve dual fixed-point iterations on large models?"*
21. *"Can your C API be embedded into Python multiprocessing without GIL or threading deadlocks?"*
22. *"Why does your solver refuse non-convex QPs instead of providing a local stationary point with optimality bounds?"*
23. *"How does your reliability branching select pseudocost initializations for unobserved variables?"*
24. *"What is the cache-miss penalty of your CSR/CSC matrix-vector multiplication on modern AVX-512 CPUs?"*
25. *"Can your solver execute inside an air-gapped refinery SCADA network without internet access?"*

---

## 16. How We Can Beat SANKHYA: Strategic Opportunities & Top 3 Differentiators

### Top 3 Winning Differentiators for Our Team

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                               TOP 3 STRATEGIC DIFFERENTIATORS                          │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                        │
│ 1. GENUINE GPU ACCELERATION (CUDA SpMV + First-Order PDHG Kernels)                     │
│    • SANKHYA's biggest flaw: 0% GPU implementation despite the PS title.              │
│    • Our edge: Real CUDA C++ kernels executing on NVIDIA tensor/CUDA cores,            │
│      delivering verified 10x-50x speedups on large-scale refinery planning models.      │
│                                                                                        │
│ 2. REFINERY DIGITAL TWIN & REAL-TIME WEB DASHBOARD                                     │
│    • SANKHYA's flaw: Pure CLI / text terminal only ("No GUI").                         │
│    • Our edge: Modern web control dashboard built for MRPL refinery planners           │
│      with live crude blending simulation, shadow price calculators, and live telemetry. │
│                                                                                        │
│ 3. ADAPTIVE CPU-GPU HYBRID SOLVER ENGINE                                               │
│    • SANKHYA's flaw: Rigid manual algorithm switch (`--option algorithm=...`).         │
│    • Our edge: Intelligent classifier that profiles matrix density, condition number,   │
│      and sparsity, automatically dispatching to CPU Simplex or GPU PDHG.               │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 17. Our Proposed Solution Architecture: VAJRA (वज्र)

- **Product Name:** **VAJRA (वज्र)** — *High-Performance GPU-Accelerated Sovereign Optimization Engine & Refinery Digital Twin*
- **One-Line Pitch:** *An indigenous, GPU-native mathematical solver providing provable enterprise-scale LP/MILP/QP optimization with real-time digital twin analytics for Indian refineries.*

```
                                  USER INTERFACES
      ┌─────────────────────────────────┬─────────────────────────────────┐
      │   MRPL Refinery Web Studio      │    High-Performance Python      │
      │   (Interactive Digital Twin)    │     & C++ API / Native CLI      │
      └─────────────────────────────────┴─────────────────────────────────┘
                                        │
                                        ▼
      ┌───────────────────────────────────────────────────────────────────┐
      │               VAJRA ADAPTIVE DISPATCH ENGINE                      │
      │   Matrix Density & Condition Profiler · Automatic Architecture    │
      └─────────────────┬───────────────────────────────┬─────────────────┘
                        │                               │
       Small/Medium LP  │                               │ Large Sparse LP
       & Tight MILP     ▼                               ▼ (> 10,000 vars)
     ┌────────────────────────────┐          ┌────────────────────────────┐
     │    CPU SIMPLEX / MIP CORE  │          │   GPU ACCELERATED CORE     │
     │ • Revised Dual Simplex     │          │ • Custom CUDA SpMV Kernels │
     │ • Sparse Markowitz LU      │          │ • cuSPARSE / cuBLAS Pipes  │
     │ • Gomory & MIR Cuts        │          │ • GPU Restarted PDHG /     │
     │ • Multi-Threaded B&B Tree  │          │   Parallel ADMM Engine     │
     └─────────────┬──────────────┘          └─────────────┬──────────────┘
                   │                                       │
                   └───────────────────┬───────────────────┘
                                       │
                                       ▼
     ┌────────────────────────────────────────────────────────────────────┐
     │            INDEPENDENT DUALITY & KKT CERTIFICATE AUDITOR           │
     │   Farkas Vector Validation · KKT Complementarity Check · Ray Audit │
     └────────────────────────────────────────────────────────────────────┘
```

---

## 18. Head-to-Head Battle Plan: SANKHYA vs. VAJRA

| Dimension | Competitor: SANKHYA | Our Solution: VAJRA | Evaluator Impact |
| :--- | :--- | :--- | :--- |
| **GPU Acceleration** | **0% (Pure CPU fallback)** | **True CUDA C++ SpMV & PDHG kernels** | **Decisive Win:** Fulfills core PS title |
| **Industrial UI / UX**| None (Terminal CLI only) | **Full Web UI + Refinery Digital Twin** | **Decisive Win:** Instant visual appeal |
| **Solver Dispatch** | Manual `--option algorithm=...` | **Adaptive CPU/GPU Auto-Routing** | **Win:** Smarter engineering |
| **LP Simplex** | Revised Dual Simplex | Revised Dual Simplex + Devex | Parity |
| **Interior Point** | Mehrotra (no crossover) | Mehrotra + Vertex Crossover | **Win:** Usable shadow prices |
| **MIP Capabilities**| Basic B&B (9/30 MIPLIB) | B&B + Root/Tree Gomory & MIR Cuts | **Win:** Higher MIPLIB pass rate |
| **Verification** | Python independent verifier | Visual Real-Time KKT Audit in UI | **Win:** Accessible to all judges |
| **Benchmark Scale** | 78/89 Netlib (CPU only) | Netlib + GPU 100k+ Var Benchmarks | **Win:** Demonstrates GPU speedup |

---

## 19. Implementation Roadmap

```
PHASE 1: Core Solver & Engine Foundation
├── C++20 Model representation & Sparse Matrix (CSC/CSR)
├── MPS & LP Format File Readers
└── Revised Dual Simplex with Devex Pricing on CPU

PHASE 2: GPU Native Acceleration Engine (Crucial Differentiator)
├── CUDA Sparse Matrix-Vector Multiplication (SpMV) Kernels
├── First-Order Restarted PDHG on GPU with cuBLAS integration
└── Benchmarking GPU speedup vs CPU baseline on 10k-100k variable models

PHASE 3: Industrial Digital Twin & Web Dashboard
├── Interactive MRPL Crude Blending Digital Twin (Tank simulation)
├── Real-time Objective, Duality Gap, and Residual Charts
└── Solver Workbench with model editor and one-click execution

PHASE 4: Advanced Discrete Optimization (MILP)
├── Branch & Bound with reliability branching
├── Gomory Mixed-Integer cuts & MIR cut generator
└── Dual Simplex warm-start propagation across nodes

PHASE 5: Verification Spine & Presentation Preparation
├── Independent KKT and Farkas certificate validator
├── Reproducible benchmark CSV generation
└── Polished presentation deck highlighting GPU speedup and MRPL ROI
```

---

## 20. Final Verdict

SANKHYA is a solid, academically honest CPU linear programming solver with excellent mathematical validation. However, **its complete omission of GPU acceleration is a critical flaw against Problem Statement SIH26119.**  
By implementing real CUDA kernels, adding basis crossover, and presenting an interactive Refinery Digital Twin Web Dashboard, our team has a clear path to win the competition.

---

# ONE-PAGE BATTLE CARD

### COMPETITOR: SANKHYA
- **Core Idea:** Sovereign C++20 optimization solver written from scratch without 3rd-party solver dependencies.
- **Current Maturity:** Advanced CPU research prototype (Maturity Score: 3.3 / 5).
- **Strongest Capability:** Bounded Dual Simplex, independent Python KKT verifier, and mathematical certificates (Farkas/Ray).
- **Biggest Weakness:** **ZERO GPU acceleration.** `--gpu` flag is a stub that falls back to CPU. Slow MILP solver.
- **Biggest Risk to Us:** Their LP solver is mathematically sound and passes 78/89 Netlib benchmarks.

### OUR TEAM: VAJRA
- **Core Idea:** Indigenous, GPU-native mathematical solver with real CUDA acceleration and an MRPL Refinery Digital Twin.
- **Unique Advantage:** **Actual GPU acceleration (CUDA SpMV & PDHG)** + **Interactive Web Dashboard**.
- **First Feature to Build:** GPU-accelerated sparse matrix-vector multiplication and PDHG kernel.
- **Strongest Demo:** Live interactive MRPL Crude Blending Digital Twin solving on GPU in under 100 milliseconds with live KKT certificate verification.
- **Most Important Benchmark:** Netlib LP pass rate + GPU vs CPU speedup curve showing 10x-50x acceleration above 50,000 variables.
- **Biggest Technical Risk:** Ensuring GPU numerical stability matches double-precision CPU simplex accuracy.

---

### Final Evaluator Question
> **"If you were an SIH evaluator and had to choose only a few teams from 500+ teams, what concrete evidence would make our solution stand out from SANKHYA?"**

1. **Concrete Evidence #1: Active GPU Kernel Profiling.**  
   When the evaluator asks to see GPU usage, SANKHYA will show `0% GPU utilization` in `nvidia-smi`. Our solution will display active CUDA kernel execution with real GPU core utilization and a reproducible benchmark showing significant speedup on large sparse refinery models.
2. **Concrete Evidence #2: Control-Room Usability for MRPL.**  
   While SANKHYA requires typing command-line flags in a terminal, our solution provides an interactive refinery digital twin where judges can manipulate crude oil streams, test sulphur limits, and immediately observe optimal margins and shadow prices.
3. **Concrete Evidence #3: Mathematical Integrity with Live Proofs.**  
   Like SANKHYA, we deliver mathematically verified solutions with Farkas certificates and independent KKT checks, but we render them visually so evaluators can inspect feasibility and optimality at a glance.
