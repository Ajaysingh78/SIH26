# SANKHYA — REPRODUCIBLE BENCHMARK LABORATORY

## 1. Overview & Objective

The **SANKHYA Benchmark Laboratory** (`bench/laboratory.py`) is an engineering evidence system for:
- Mathematical correctness & numerical reliability
- Continuous Linear Programming (LP) performance
- Mixed-Integer Linear Programming (MILP) performance
- Quadratic Programming (QP & MIQP) with Dorn duality verification
- Adaptive CPU/GPU solver routing validation
- Industrial MRPL Refinery Digital Twin decision workloads

The core design criterion of this laboratory is:

> *"Can another person run the same benchmark configuration and reproduce the reported result?"*

This is an **engineering evidence system**, NOT a marketing benchmark. It does not fabricate numbers, does not omit failing checks, and explicitly captures host hardware and environment constraints.

---

## 2. Benchmark Matrix

The laboratory evaluates optimization problems across four distinct classes:

| Suite | Problem Class | Instances | Characteristics | Ground Truth Reference |
| :--- | :--- | :--- | :--- | :--- |
| **Continuous LP** | Linear Programming | `afiro`, `sc50a`, `share2b`, `blend`, `adlittle`, `crude_blend` | Small-to-medium Netlib standard LPs & refinery blend models | Netlib published readme (`data/netlib/reference.json`) |
| **Mixed-Integer** | Mixed-Integer Linear | `blend_milp`, `lot_sizing` | Discrete decisions, unit commitments, lot-sizing | Independent B&B dual bound proofs |
| **Quadratic** | Convex QP & MIQP | `qp_blend`, `crude_blend_qp`, `miqp_blend` | Quadratic price penalties, convex objectives, integer batches | Analytical KKT points & Dorn dual checks |
| **Refinery Twin** | Industrial Optimization | `baseline`, `high_demand`, `limited_crude`, `unit_constraint`, `quality_constraint`, `infeasible_demand` | 32-constraint, 39-variable synthetic MRPL refinery planning twin | Feature 4 digital twin calibrated KPIs & Farkas certificates |

---

## 3. Reproducibility Metadata

Every benchmark run records the following metadata alongside results:

- **Git Commit Hash**: Exact commit hash and `-dirty` suffix if tracked workspace files are modified.
- **Host OS & Architecture**: Operating system version and processor architecture (e.g. `Windows-AMD64`, `Linux-x86_64`).
- **CPU Specifications**: Processor family, identifier, and logical core count.
- **GPU & CUDA Status**: Probed via `nvidia-smi`. If unavailable, recorded truthfully as `None (CPU Fallback)`. No GPU numbers or speedups are ever simulated or fabricated.
- **Toolchain & Python Environment**: Python runtime and NumPy version.
- **Timestamps**: ISO-8601 UTC timestamp for provenance tracking.

---

## 4. Correctness First: Independent Verification

A benchmark run is never marked successful solely because the process exited with code 0.

### 4.1 Solution Auditing
1. **Model Parsing**: Models are parsed independently from standard fixed-format or free-format MPS files.
2. **Independent Verification (`tools/verify_solution.py`)**:
   - **Primal Feasibility**: Variable bounds ($l \le x \le u$) and row activities ($l_r \le A x \le u_r$).
   - **Dual Feasibility**: Multipliers $y$ for constraints and reduced costs $d = c - A^T y$.
   - **Complementary Slackness**: Multiplier-slack products $|y_i| \cdot s_i \le \epsilon$.
   - **Strong Duality**: Primal-dual objective gap within tolerance.
   - **Dorn Dual for QP**: Dual objective accounts for $0.5 x^T Q x$ curvature offset.
   - **MIP Bound Proofs**: Branch-and-bound incumbent tested against dual bound and integer tolerances.
   - **Infeasibility Proofs**: Farkas certificates verify that no primal point exists.

### 4.2 Status Categorization
Every result explicitly distinguishes:
- `OPTIMAL`: Optimal solution found and verified.
- `FEASIBLE`: Primal feasible solution found within bounds.
- `INFEASIBLE`: Model provably infeasible (verified via dual ray/Farkas).
- `UNBOUNDED`: Objective improves indefinitely without bound.
- `FAILED`: Numerical failure, ill-conditioning, or solver error.
- `TIMEOUT`: Execution exceeded allocated time limit.
- `UNVERIFIED`: Solver claimed optimality but independent verification failed or was skipped.

---

## 5. CPU vs. GPU Performance & Adaptive Routing

### 5.1 Real Hardware vs. Truthful Fallback
- When GPU hardware and CUDA execution are present: comparable workloads are executed on CPU and GPU.
- When GPU hardware is absent:
  - CPU benchmarks execute normally.
  - Requesting `--backend gpu` executes `CPU (Fallback)` and clearly reports `actual_backend: "CPU (Fallback)"`.
  - Zero GPU speedup is claimed without actual physical measurements.

### 5.2 Adaptive Routing Validation (AUTO Mode)
The problem analyzer analyzes problem dimensions, matrix sparsity, and hardware availability:
- **Small/Moderate Dense Models**: Low setup latency favours CPU revised simplex.
- **Large Dense Models (Rows $\ge 1000$ or NNZ $\ge 100,000$)**: Routed to GPU acceleration when CUDA is available.
- **Validation**: AUTO execution is recorded with routing reason and evaluated against direct CPU execution.

---

## 6. Repeated Runs & Statistical Metrics

Solver timings are subject to CPU clock fluctuations and OS background activity. To provide trustworthy engineering figures:
- The laboratory supports repeated runs via `--repeats N`.
- Metrics captured:
  - Individual runtimes ($[t_1, t_2, \dots, t_N]$)
  - `mean_seconds`: Arithmetic mean
  - `median_seconds`: Median runtime (preferred robust estimator)
  - `min_seconds` & `max_seconds`: Range envelope
  - `std_dev_seconds`: Standard deviation
  - `deterministic`: Boolean check confirming objective variance is zero ($\max(\text{obj}) - \min(\text{obj}) \le 10^{-7}$).

---

## 7. Result Comparison & Regression Detection

### 7.1 Comparison Mechanism (`--compare RUN_A.json RUN_B.json`)
Compares two benchmark executions across:
- **Performance**: Runtime ratio ($T_A / T_B$) and shifted geometric mean speedup ($s = 0.01$).
- **Numerical Difference**: Absolute objective difference ($|\text{obj}_A - \text{obj}_B|$) and relative discrepancy.
- **Correctness Parity**: Status agreement and independent verification agreement.

### 7.2 Regression Detection (`--check-regression BASELINE.json`)
Catches software regressions during continuous integration:
- **Status Regression**: An instance previously `OPTIMAL` becoming `FAILED` or `TIMEOUT`.
- **Verification Regression**: An instance previously `VERIFIED` becoming `REJECTED`.
- **Objective Discrepancy**: Objective value drifting by more than `--threshold-obj` (default: $10^{-4}$).
- **Runtime Slowdown**: Runtime degrading by more than `--threshold-slowdown` (default: $20\%$).

---

## 8. Benchmark Commands

All commands are executed from the repository root:

### 1. Run a Single Instance
```bash
python bench/laboratory.py --instance afiro
python bench/laboratory.py --instance crude_blend
python bench/laboratory.py --instance baseline
```

### 2. Run a Full Benchmark Suite
```bash
# Continuous Netlib LP suite
python bench/laboratory.py --suite netlib

# Mixed-Integer MILP suite
python bench/laboratory.py --suite milp

# Quadratic QP & MIQP suite
python bench/laboratory.py --suite qp

# MRPL Industrial Refinery Digital Twin scenarios
python bench/laboratory.py --suite refinery

# All benchmark suites
python bench/laboratory.py --suite all
```

### 3. Specify Target Backend
```bash
# Force CPU execution
python bench/laboratory.py --suite all --backend cpu

# Request GPU execution (truthfully reports Fallback if no CUDA GPU)
python bench/laboratory.py --suite all --backend gpu

# Adaptive AUTO solver routing
python bench/laboratory.py --suite all --backend auto
```

### 4. Repeated Runs for Timing Statistics
```bash
python bench/laboratory.py --suite refinery --repeats 5
```

### 5. Compare Two Benchmark Runs
```bash
python bench/laboratory.py --compare bench/results/run_cpu.json bench/results/run_gpu.json
```

### 6. Automated Regression Check
```bash
python bench/laboratory.py --suite refinery --check-regression bench/results/baseline_refinery.json
```

### 7. Custom Export Paths
```bash
python bench/laboratory.py --suite all --export-json results.json --export-csv results.csv --export-md report.md
```

### 8. Run Automated Test Suite
```bash
python -m unittest bench/test_laboratory.py
```

---

## 9. Environment Limitations & Non-Claims

1. **Environment Dependence**: Wall-clock runtimes depend directly on host processor speed, thermal throttling, memory bandwidth, and operating system scheduling.
2. **GPU Availability**: SANKHYA will not report GPU speedup on machines lacking physical NVIDIA CUDA hardware.
3. **Synthetic Refinery Data**: All refinery yields, crude costs, and specifications represent synthetic operational scenarios inspired by MRPL configurations for algorithm demonstration, not proprietary operational data.
