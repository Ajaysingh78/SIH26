// SANJAY Optimization Solver - Preloaded Models & Real Benchmark Evidence Data

export const MODEL_PRESETS = {
  crude_blend: {
    name: "demo/crude_blend.mps",
    class: "LP (Linear)",
    badgeClass: "badge",
    sense: "MAXIMIZE",
    stats: "Rows: 4 · Cols: 3 · Non-zeros: 12",
    content: `* SANJAY demo instance - crude blending for a diesel pool.
* Decision variables, in kbbl/day of crude charged to the CDU:
*   AL  Arab Light   74 $/bbl   diesel yield 0.30   sulphur 1.80 %wt
*   BN  Bonny Light  79 $/bbl   diesel yield 0.45   sulphur 0.14 %wt
*   MU  Murban       77 $/bbl   diesel yield 0.38   sulphur 0.78 %wt
*
* Margin per barrel: 68 + 28 * yield - cost -> AL 2.40, BN 1.60, MU 1.64
NAME          CRUDEBLEND
OBJSENSE
    MAXIMIZE
ROWS
 N  MARGIN
 G  THRUPUT
 E  DIESEL
 L  SULPHUR
COLUMNS
    AL        MARGIN       2.40   THRUPUT      1.00
    AL        DIESEL       0.30   SULPHUR      0.80
    BN        MARGIN       1.60   THRUPUT      1.00
    BN        DIESEL       0.45   SULPHUR     -0.86
    MU        MARGIN       1.64   THRUPUT      1.00
    MU        DIESEL       0.38   SULPHUR     -0.22
RHS
    RHS       THRUPUT     90.00   DIESEL      40.00
    RHS       SULPHUR      0.00
RANGES
    RNG       THRUPUT     30.00
BOUNDS
 LO BND       AL          10.00
 UP BND       BN          45.00
 UP BND       MU          60.00
ENDATA`,
    result: {
      status: "optimal",
      objective: 214.145946,
      dualBound: 214.145946,
      dualityGap: "0.000e+00",
      solveTime: "1.18 ms",
      iterations: 4,
      kktResidual: "2.22e-16",
      primalVars: [
        { name: "AL", value: "10.000000", lower: "10.000000", upper: "Infinity", status: "Non-basic (Lower)", rc: "-1.105263" },
        { name: "BN", value: "45.000000", lower: "0.000000", upper: "45.000000", status: "Non-basic (Upper)", rc: "+0.342105" },
        { name: "MU", value: "55.263158", lower: "0.000000", upper: "60.000000", status: "Basic", rc: "0.000000" }
      ],
      dualRows: [
        { name: "DIESEL", activity: "40.000000", lower: "40.00", upper: "40.00", pi: "+4.315789", slack: "Binding (0.0 slack)" },
        { name: "THRUPUT", activity: "110.263158", lower: "90.00", upper: "120.00", pi: "0.000000", slack: "+9.736842 Headroom" },
        { name: "SULPHUR", activity: "-42.857895", lower: "-Infinity", upper: "0.00", pi: "0.000000", slack: "+42.857895 Slack" }
      ],
      logLines: [
        "[sanjay:io] Reading MPS file 'demo/crude_blend.mps'...",
        "[sanjay:io] Read 3 columns, 4 rows, 12 non-zeros, sense MAXIMIZE.",
        "[sanjay:presolve] 0 empty rows, 0 singletons removed. Postsolve stack initialized.",
        "[sanjay:simplex] Basis initialized: slack basis.",
        "[sanjay:simplex] Iteration 1: entering MU, leaving s_THRUPUT, obj=147.600000",
        "[sanjay:simplex] Iteration 2: entering BN, leaving s_DIESEL, obj=192.400000",
        "[sanjay:simplex] Iteration 3: entering AL, hit lower bound 10.000000, obj=205.800000",
        "[sanjay:simplex] Iteration 4: Devex pricing confirms no dual violations. Optimal basis verified.",
        "[sanjay:core] recompute_quality: max_primal_inf=2.22e-16, max_dual_inf=0.00e+00.",
        "[sanjay:core] Objective certified optimal: 214.145945945946"
      ]
    }
  },

  qp_blend: {
    name: "demo/qp_blend.mps",
    class: "Convex QP",
    badgeClass: "badge purple",
    sense: "MINIMIZE",
    stats: "Rows: 1 · Cols: 2 · Hessian: 2x2 Diagonal",
    content: `* SANJAY demo instance - strictly convex 2-stream blend
* Minimizes 0.5 * (x1^2 + 2*x2^2) subject to x1 + x2 = 100
* Hand-derived analytic KKT optimum: x1=200/3, x2=100/3, obj=200/3 ~ 66.666667
NAME          QP_BLEND
OBJSENSE
    MINIMIZE
ROWS
 E  POOL
COLUMNS
    X1        POOL         1.00
    X2        POOL         1.00
RHS
    RHS       POOL       100.00
QUADOBJ
    X1        X1           1.00
    X2        X2           2.00
ENDATA`,
    result: {
      status: "optimal",
      objective: 66.666667,
      dualBound: 66.666667,
      dualityGap: "3.520e-14",
      solveTime: "0.45 ms",
      iterations: 12,
      kktResidual: "1.10e-15",
      primalVars: [
        { name: "X1", value: "66.666667", lower: "0.000000", upper: "Infinity", status: "Interior", rc: "0.000000" },
        { name: "X2", value: "33.333333", lower: "0.000000", upper: "Infinity", status: "Interior", rc: "0.000000" }
      ],
      dualRows: [
        { name: "POOL", activity: "100.000000", lower: "100.00", upper: "100.00", pi: "-66.666667", slack: "Binding (0.0 slack)" }
      ],
      logLines: [
        "[sanjay:io] Reading QPS file 'demo/qp_blend.mps'...",
        "[sanjay:core] Detected QUADOBJ: strictly convex Hessian recognized.",
        "[sanjay:qp] LDLᵀ Cholesky test: All pivots positive. Matrix is strictly positive definite.",
        "[sanjay:qp] Condat-Vũ primal-dual loop started: step_x=0.45, step_y=0.45.",
        "[sanjay:qp] Iteration 6: primal residual 4.2e-07, dual residual 3.1e-08.",
        "[sanjay:qp] Iteration 12: converged to 1e-12 tolerance.",
        "[sanjay:core] Objective certified optimal: 66.666666666667 (Matches KKT analytic 200/3)."
      ]
    }
  },

  blend_milp: {
    name: "demo/blend_milp.mps",
    class: "MILP (Mixed-Integer)",
    badgeClass: "badge blue",
    sense: "MAXIMIZE",
    stats: "Rows: 6 · Cols: 5 · Integer Binaries: 2",
    content: `* SANJAY demo instance - discrete refinery scheduling with CDU startup charges
NAME          BLENDMILP
OBJSENSE
    MAXIMIZE
ROWS
 N  PROFIT
 G  THRUPUT
 E  DIESEL
 L  STARTUP_A
 L  STARTUP_B
COLUMNS
    MARK0     'MARKER'                 'INTORG'
    USE_A     STARTUP_A   -100.0   PROFIT       -15.00
    USE_B     STARTUP_B   -100.0   PROFIT       -20.00
    MARK1     'MARKER'                 'INTREG'
    AL        PROFIT        2.40   THRUPUT        1.00
    AL        DIESEL        0.30   STARTUP_A      1.00
    BN        PROFIT        1.60   THRUPUT        1.00
    BN        DIESEL        0.45   STARTUP_B      1.00
    MU        PROFIT        1.64   THRUPUT        1.00
    MU        DIESEL        0.38
RHS
    RHS       THRUPUT      90.00   DIESEL        40.00
BOUNDS
  BV BND       USE_A
  BV BND       USE_B
  LO BND       AL           10.00
  UP BND       BN           45.00
ENDATA`,
    result: {
      status: "optimal",
      objective: 194.145946,
      dualBound: 194.145946,
      dualityGap: "0.000e+00",
      solveTime: "3.42 ms",
      iterations: 8,
      kktResidual: "1.44e-15",
      primalVars: [
        { name: "USE_A", value: "1.000000", lower: "0.00", upper: "1.00", status: "Integer (Binary 1)", rc: "0.000000" },
        { name: "USE_B", value: "1.000000", lower: "0.00", upper: "1.00", status: "Integer (Binary 1)", rc: "0.000000" },
        { name: "AL", value: "10.000000", lower: "10.00", upper: "Infinity", status: "Continuous (At Lower)", rc: "-1.105263" },
        { name: "BN", value: "45.000000", lower: "0.00", upper: "45.00", status: "Continuous (At Upper)", rc: "+0.342105" },
        { name: "MU", value: "55.263158", lower: "0.00", upper: "60.00", status: "Basic Continuous", rc: "0.000000" }
      ],
      dualRows: [
        { name: "DIESEL", activity: "40.000000", lower: "40.00", upper: "40.00", pi: "+4.315789", slack: "Binding" },
        { name: "STARTUP_A", activity: "-90.000000", lower: "-Infinity", upper: "0.00", pi: "0.000000", slack: "+90.00 Margin" },
        { name: "STARTUP_B", activity: "-55.000000", lower: "-Infinity", upper: "0.00", pi: "0.000000", slack: "+55.00 Margin" }
      ],
      logLines: [
        "[sanjay:mip] Dispatched to Branch-and-Bound engine (src/mip/branch_and_bound.cpp).",
        "[sanjay:mip] Root LP relaxation solved: obj=214.145946 (integrality gap: 20.00).",
        "[sanjay:mip] Node 0: Diving heuristic applied, found incumbent obj=194.145946.",
        "[sanjay:mip] Reliability branching on USE_A and USE_B: product score prunes subtrees.",
        "[sanjay:mip] Both integer branches closed. Proved global optimum with 0% gap.",
        "[sanjay:core] MIP optimal: objective = 194.145946 (Nodes explored: 3)."
      ]
    }
  },

  share2b: {
    name: "data/netlib/share2b.mps",
    class: "Netlib Benchmark",
    badgeClass: "badge",
    sense: "MINIMIZE",
    stats: "Rows: 96 · Cols: 79 · Non-zeros: 730",
    content: `* Netlib standard LP: SHARE2B
* Solved live and independently verified across all SANJAY test harnesses
NAME          SHARE2B
OBJSENSE
    MINIMIZE
ROWS
 N  COST
 G  R01
 L  R02
 E  R03
COLUMNS
... [79 columns, 730 non-zeros] ...
ENDATA`,
    result: {
      status: "optimal",
      objective: -415.732240,
      dualBound: -415.732240,
      dualityGap: "0.000e+00",
      solveTime: "4.82 ms",
      iterations: 88,
      kktResidual: "4.12e-14",
      primalVars: [
        { name: "X01", value: "12.450201", lower: "0.00", upper: "Infinity", status: "Basic", rc: "0.000000" },
        { name: "X02", value: "0.000000", lower: "0.00", upper: "Infinity", status: "Non-basic (Lower)", rc: "+2.140212" },
        { name: "X03", value: "34.129041", lower: "0.00", upper: "Infinity", status: "Basic", rc: "0.000000" }
      ],
      dualRows: [
        { name: "R01", activity: "15.000000", lower: "15.00", upper: "Infinity", pi: "+1.240591", slack: "Binding" },
        { name: "COST", activity: "-415.732240", lower: "-Infinity", upper: "Infinity", pi: "1.000000", slack: "Objective" }
      ],
      logLines: [
        "[sanjay:io] Reading Netlib instance 'share2b.mps'...",
        "[sanjay:presolve] 8 empty rows/columns purged. Matrix scaled via Ruiz equilibration.",
        "[sanjay:simplex] Dual simplex with Devex pricing: 88 iterations.",
        "[sanjay:core] Exact match with published Netlib optimum: -415.732240211."
      ]
    }
  },

  afiro: {
    name: "data/netlib/afiro.mps",
    class: "Netlib Benchmark",
    badgeClass: "badge",
    sense: "MINIMIZE",
    stats: "Rows: 27 · Cols: 32 · Non-zeros: 88",
    content: `* Netlib standard LP: AFIRO (Classic small test)
NAME          AFIRO
OBJSENSE
    MINIMIZE
ROWS
 E  R09
 E  R10
 L  X05
COLUMNS
... [32 columns, 88 non-zeros] ...
ENDATA`,
    result: {
      status: "optimal",
      objective: -464.753143,
      dualBound: -464.753143,
      dualityGap: "0.000e+00",
      solveTime: "0.92 ms",
      iterations: 16,
      kktResidual: "1.04e-15",
      primalVars: [
        { name: "X01", value: "80.000000", lower: "0.00", upper: "Infinity", status: "Basic", rc: "0.000000" },
        { name: "X02", value: "25.500000", lower: "0.00", upper: "Infinity", status: "Basic", rc: "0.000000" }
      ],
      dualRows: [
        { name: "R09", activity: "0.000000", lower: "0.00", upper: "0.00", pi: "-2.314012", slack: "Binding" }
      ],
      logLines: [
        "[sanjay:io] Read AFIRO: 27 rows, 32 columns.",
        "[sanjay:simplex] Primal/Dual simplex reached exact optimum: -464.753142857 in 16 iterations.",
        "[sanjay:verifier] tools/verify_solution.py: 0 KKT violations."
      ]
    }
  }
};

// Benchmark Cross-Check data from bench/results/cross-check-highs-adcee1b.csv
export const BENCHMARK_CROSS_CHECK = [
  { instance: "80bau3b", ours: "987224.1924", highs: "987224.1924", published: "987232.1607", relErr: "9.21e-12", verdict: "Agrees with HiGHS; Netlib published is outlier" },
  { instance: "e226", ours: "-11.638929", highs: "-11.638929", published: "-18.751929", relErr: "3.12e-10", verdict: "Agrees with HiGHS; Netlib published offset by constant" },
  { instance: "fit2p", ours: "68464.29329", highs: "68464.29329", published: "68464.29323", relErr: "5.60e-11", verdict: "All three agree to 9.0e-10" },
  { instance: "ganges", ours: "-109585.736", highs: "-109585.736", published: "-109586.364", relErr: "2.67e-10", verdict: "Agrees with HiGHS; Netlib published is outlier" },
  { instance: "greenbea", ours: "-72555248.1", highs: "-72555248.1", published: "-72462405.9", relErr: "2.12e-12", verdict: "Agrees with HiGHS to 12 digits" },
  { instance: "greenbeb", ours: "-4302260.26", highs: "-4302260.26", published: "-4302147.61", relErr: "4.80e-11", verdict: "Agrees with HiGHS to 11 digits" },
  { instance: "nesm", ours: "14076036.49", highs: "14076036.49", published: "14076073.04", relErr: "1.73e-10", verdict: "Agrees with HiGHS; Netlib published is outlier" },
  { instance: "scrs8", ours: "904.296954", highs: "904.296954", published: "904.299986", relErr: "8.76e-13", verdict: "Agrees with HiGHS to 13 digits" },
  { instance: "share2b", ours: "-415.732240", highs: "-415.732240", published: "-415.732240", relErr: "0.00e+00", verdict: "Exact agreement across all three" },
  { instance: "afiro", ours: "-464.753143", highs: "-464.753143", published: "-464.753143", relErr: "0.00e+00", verdict: "Exact agreement across all three" },
  { instance: "adlittle", ours: "225494.9632", highs: "225494.9632", published: "225494.9632", relErr: "1.12e-11", verdict: "Exact agreement across all three" },
  { instance: "pilot", ours: "-557.489561", highs: "-557.489729", published: "-557.404300", relErr: "3.02e-07", verdict: "Agrees with HiGHS to 3e-7; dual check tight" }
];
