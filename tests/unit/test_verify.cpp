// SPDX-License-Identifier: Apache-2.0
// SANKHYA - Independent Verification Layer tests (Phase 2 Feature 3).
#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "sankhya/gpu_ops.hpp"
#include "sankhya/model.hpp"
#include "sankhya/options.hpp"
#include "sankhya/verify.hpp"

namespace sankhya {
namespace {

// Helper: 2x2 LP
// min 2x1 + 3x2
// s.t.  x1 + x2 >= 2
//       x1, x2 >= 0
Model make_simple_lp() {
  Model m;
  m.name = "simple_lp";
  m.sense = ObjSense::kMinimize;
  m.col_lower = {0.0, 0.0};
  m.col_upper = {kInfinity, kInfinity};
  m.col_cost = {2.0, 3.0};
  m.is_integer = {false, false};
  m.row_lower = {2.0};
  m.row_upper = {kInfinity};
  m.matrix.reset(1, 2);
  m.matrix.add_entry(0, 0, 1.0);
  m.matrix.add_entry(0, 1, 1.0);
  m.matrix.finalize();
  return m;
}

// Helper: 2x2 MILP
// min 2x1 + 3x2
// s.t.  x1 + x2 >= 2.5
//       x1, x2 integer in [0, 10]
Model make_simple_milp() {
  Model m = make_simple_lp();
  m.name = "simple_milp";
  m.row_lower = {2.5};
  m.is_integer = {true, true};
  return m;
}

// Helper: 2-variable Convex QP
// min 0.5 (x1^2 + x2^2) - x1 - x2
// s.t.  x1 + x2 <= 1, x >= 0
Model make_simple_qp() {
  Model m = make_simple_lp();
  m.name = "simple_qp";
  m.row_lower = {-kInfinity};
  m.row_upper = {1.0};
  m.col_cost = {-1.0, -1.0};
  m.hessian.reset(2, 2);
  m.hessian.add_entry(0, 0, 1.0);
  m.hessian.add_entry(1, 1, 1.0);
  m.hessian.finalize();
  return m;
}

}  // namespace

// =========================================================================================
// 1. CORRECT LP VERIFICATION (TASK 4, 10.1)
// =========================================================================================

TEST(VerificationTrustLayer, CorrectLpIsVerifiedOptimal) {
  const Model m = make_simple_lp();
  // Optimal solution: x = [2.0, 0.0], obj = 4.0
  Solution sol;
  sol.status = SolveStatus::kOptimal;
  sol.col_value = {2.0, 0.0};
  sol.objective = 4.0;
  sol.row_dual = {2.0};
  sol.col_dual = {0.0, 1.0};  // d = c - A'y = [2, 3] - [2, 2] = [0, 1]

  Options opts;
  const VerificationReport rep = verify_solution(m, sol, opts);

  EXPECT_TRUE(rep.passed());
  EXPECT_EQ(rep.status, VerificationStatus::kVerifiedOptimal);
  EXPECT_EQ(rep.primal_check, CheckResult::kPass);
  EXPECT_EQ(rep.dual_check, CheckResult::kPass);
  EXPECT_EQ(rep.objective_check, CheckResult::kPass);
  EXPECT_LE(rep.max_primal_violation, 1e-7);
}

// =========================================================================================
// 2. INCORRECT LP SOLUTION REJECTED (TASK 4, 10.2)
// =========================================================================================

TEST(VerificationTrustLayer, IncorrectLpSolutionIsRejected) {
  const Model m = make_simple_lp();
  // Violates constraint x1 + x2 >= 2
  Solution sol;
  sol.status = SolveStatus::kOptimal;
  sol.col_value = {0.5, 0.5};  // x1 + x2 = 1.0 < 2.0!
  sol.objective = 2.5;

  Options opts;
  const VerificationReport rep = verify_solution(m, sol, opts);

  EXPECT_FALSE(rep.passed());
  EXPECT_EQ(rep.status, VerificationStatus::kRejected);
  EXPECT_EQ(rep.primal_check, CheckResult::kFail);
  EXPECT_FALSE(rep.errors.empty());
}

// =========================================================================================
// 3. CORRECT QP VERIFICATION (TASK 4, 10.3)
// =========================================================================================

TEST(VerificationTrustLayer, CorrectQpIsVerified) {
  const Model m = make_simple_qp();
  // x = [0.5, 0.5], obj = 0.5*(0.25+0.25) - 0.5 - 0.5 = 0.25 - 1.0 = -0.75
  Solution sol;
  sol.status = SolveStatus::kOptimal;
  sol.col_value = {0.5, 0.5};
  sol.objective = -0.75;

  Options opts;
  const VerificationReport rep = verify_solution(m, sol, opts);

  EXPECT_TRUE(rep.passed());
  EXPECT_EQ(rep.status, VerificationStatus::kVerifiedOptimal);
  EXPECT_EQ(rep.primal_check, CheckResult::kPass);
  EXPECT_EQ(rep.objective_check, CheckResult::kPass);
}

// =========================================================================================
// 4. INCORRECT QP SOLUTION REJECTED (TASK 4, 10.4)
// =========================================================================================

TEST(VerificationTrustLayer, IncorrectQpSolutionIsRejected) {
  const Model m = make_simple_qp();
  // x = [1.0, 1.0] -> x1 + x2 = 2.0 > 1.0 (violates constraint!)
  Solution sol;
  sol.status = SolveStatus::kOptimal;
  sol.col_value = {1.0, 1.0};
  sol.objective = 0.0;

  Options opts;
  const VerificationReport rep = verify_solution(m, sol, opts);

  EXPECT_FALSE(rep.passed());
  EXPECT_EQ(rep.status, VerificationStatus::kRejected);
  EXPECT_EQ(rep.primal_check, CheckResult::kFail);
}

// =========================================================================================
// 5. FEASIBLE MILP VERIFIED (TASK 5, 10.5)
// =========================================================================================

TEST(VerificationTrustLayer, FeasibleMilpIsVerified) {
  const Model m = make_simple_milp();
  // Integral feasible point: x = [3.0, 0.0] -> 3.0 >= 2.5
  Solution sol;
  sol.status = SolveStatus::kOptimal;
  sol.col_value = {3.0, 0.0};
  sol.objective = 6.0;

  Options opts;
  const VerificationReport rep = verify_solution(m, sol, opts);

  EXPECT_TRUE(rep.passed());
  EXPECT_EQ(rep.integrality_check, CheckResult::kPass);
  EXPECT_EQ(rep.primal_check, CheckResult::kPass);
}

// =========================================================================================
// 6. FRACTIONAL MILP CANDIDATE REJECTED (TASK 5, 10.6)
// =========================================================================================

TEST(VerificationTrustLayer, FractionalMilpIsRejected) {
  const Model m = make_simple_milp();
  // LP relaxation point: x = [2.5, 0.0] -> x1 is fractional!
  Solution sol;
  sol.status = SolveStatus::kOptimal;
  sol.col_value = {2.5, 0.0};
  sol.objective = 5.0;

  Options opts;
  const VerificationReport rep = verify_solution(m, sol, opts);

  EXPECT_FALSE(rep.passed());
  EXPECT_EQ(rep.status, VerificationStatus::kRejected);
  EXPECT_EQ(rep.integrality_check, CheckResult::kFail);
}

// =========================================================================================
// 7. INCORRECT OBJECTIVE REJECTED (TASK 3, 10.7)
// =========================================================================================

TEST(VerificationTrustLayer, IncorrectObjectiveDiscrepancyIsRejected) {
  const Model m = make_simple_lp();
  Solution sol;
  sol.status = SolveStatus::kOptimal;
  sol.col_value = {2.0, 0.0};
  sol.objective = 999.0;  // False objective claimed!

  Options opts;
  const VerificationReport rep = verify_solution(m, sol, opts);

  EXPECT_FALSE(rep.passed());
  EXPECT_EQ(rep.status, VerificationStatus::kRejected);
  EXPECT_EQ(rep.objective_check, CheckResult::kFail);
}

// =========================================================================================
// 8. CONSTRAINT VIOLATION REJECTED (TASK 3, 10.8)
// =========================================================================================

TEST(VerificationTrustLayer, ConstraintViolationIsRejected) {
  const Model m = make_simple_lp();
  Solution sol;
  sol.status = SolveStatus::kOptimal;
  sol.col_value = {0.0, 0.0};  // Ax = 0 < 2
  sol.objective = 0.0;

  Options opts;
  const VerificationReport rep = verify_solution(m, sol, opts);

  EXPECT_FALSE(rep.passed());
  EXPECT_EQ(rep.primal_check, CheckResult::kFail);
}

// =========================================================================================
// 9. BOUND VIOLATION REJECTED (TASK 3, 10.9)
// =========================================================================================

TEST(VerificationTrustLayer, NegativeVariableViolatingLowerBoundIsRejected) {
  const Model m = make_simple_lp();
  Solution sol;
  sol.status = SolveStatus::kOptimal;
  sol.col_value = {-1.0, 3.0};  // x1 < 0 violating lower bound!
  sol.objective = 7.0;

  Options opts;
  const VerificationReport rep = verify_solution(m, sol, opts);

  EXPECT_FALSE(rep.passed());
  EXPECT_EQ(rep.primal_check, CheckResult::kFail);
}

// =========================================================================================
// 10. NUMERICAL TOLERANCE BOUNDARY (TASK 8, 10.10)
// =========================================================================================

TEST(VerificationTrustLayer, NumericalToleranceBoundaryAcceptanceAndRejection) {
  const Model m = make_simple_lp();

  // Exactly within 1e-8 tolerance
  Solution sol_pass;
  sol_pass.status = SolveStatus::kOptimal;
  sol_pass.col_value = {2.0 - 5e-8, 0.0};
  sol_pass.objective = 4.0 - 1e-7;

  Options opts;
  opts.set_double("primal_feasibility_tolerance", 1e-7);
  const VerificationReport rep_pass = verify_solution(m, sol_pass, opts);
  EXPECT_TRUE(rep_pass.passed());

  // Exceeding 1e-7 tolerance
  Solution sol_fail;
  sol_fail.status = SolveStatus::kOptimal;
  sol_fail.col_value = {2.0 - 2e-6, 0.0};
  sol_fail.objective = 4.0 - 4e-6;

  const VerificationReport rep_fail = verify_solution(m, sol_fail, opts);
  EXPECT_FALSE(rep_fail.passed());
}

// =========================================================================================
// 11. GPU / REFERENCE MISMATCH REJECTED (TASK 6, 10.11)
// =========================================================================================

TEST(VerificationTrustLayer, GpuReferenceMismatchIsRejected) {
  const std::vector<double> gpu_res = {1.0, 2.0, 3.0};
  const std::vector<double> cpu_ref = {1.0, 2.0005, 3.0};  // 5e-4 difference!

  const auto cmp = gpu::compare_vectors(3, gpu_res.data(), cpu_ref.data(), 1e-7);
  EXPECT_FALSE(cmp.passed);
  EXPECT_GT(cmp.max_abs_diff, 1e-7);
}

// =========================================================================================
// 12. VALID GPU / REFERENCE ACCEPTED (TASK 6, 10.12)
// =========================================================================================

TEST(VerificationTrustLayer, ValidGpuReferenceIsAccepted) {
  const std::vector<double> gpu_res = {1.0, 2.0, 3.0};
  const std::vector<double> cpu_ref = {1.0, 2.0 + 1e-9, 3.0};

  const auto cmp = gpu::compare_vectors(3, gpu_res.data(), cpu_ref.data(), 1e-7);
  EXPECT_TRUE(cmp.passed);
  EXPECT_LE(cmp.max_abs_diff, 1e-7);
}

// =========================================================================================
// 13. AUTO CPU FALLBACK REPORTED (TASK 7, 10.13)
// =========================================================================================

TEST(VerificationTrustLayer, AutoCpuFallbackIsCorrectlyReported) {
  const Model m = make_simple_lp();
  Solution sol;
  sol.status = SolveStatus::kOptimal;
  sol.col_value = {2.0, 0.0};
  sol.objective = 4.0;
  sol.algorithm = "simplex-primal";

  Options opts;
  opts.set_string("backend", "auto");
  opts.set_bool("gpu", true);  // legacy flag requested, but run on CPU

  const VerificationReport rep = verify_solution(m, sol, opts);
  EXPECT_TRUE(rep.passed());
  EXPECT_TRUE(rep.fallback_occurred);
  EXPECT_FALSE(rep.gpu_executed);
}

// =========================================================================================
// 14. EXPLICIT GPU FAILURE NOT MISLABELED AS SUCCESS (TASK 7, 10.14)
// =========================================================================================

TEST(VerificationTrustLayer, ExplicitGpuFailureNotMislabeled) {
  Solution sol;
  sol.status = SolveStatus::kNotSolved;
  sol.algorithm = "none";
  sol.message = "Explicitly requested GPU backend (--backend=gpu), but CUDA device is not available";

  const Model m = make_simple_lp();
  Options opts;
  opts.set_string("backend", "gpu");

  const VerificationReport rep = verify_solution(m, sol, opts);
  EXPECT_EQ(rep.status, VerificationStatus::kNotApplicable);
  EXPECT_FALSE(rep.gpu_executed);
}

// =========================================================================================
// 15. EXISTING CERTIFICATE BEHAVIOR PRESERVED (TASK 9, 10.15)
// =========================================================================================

TEST(VerificationTrustLayer, FarkasInfeasibleCertificateVerified) {
  // Infeasible model: x >= 2 and x <= 1
  Model m;
  m.sense = ObjSense::kMinimize;
  m.col_lower = {0.0};
  m.col_upper = {kInfinity};
  m.col_cost = {1.0};
  m.is_integer = {false};
  m.row_lower = {2.0, -kInfinity};
  m.row_upper = {kInfinity, 1.0};
  m.matrix.reset(2, 1);
  m.matrix.add_entry(0, 0, 1.0);  // row 0: x >= 2
  m.matrix.add_entry(1, 0, 1.0);  // row 1: x <= 1
  m.matrix.finalize();

  Solution sol;
  sol.status = SolveStatus::kInfeasible;
  // Multipliers: y0 = 1.0 (requires lower bound 2), y1 = -1.0 (requires upper bound 1)
  // S = 1*2 + (-1)*1 = 1 > 0
  // d = A'y = 1*1 + (-1)*1 = 0. Column box max d*x = 0. Since 0 < 1, no x exists!
  sol.farkas_dual = {1.0, -1.0};

  Options opts;
  const VerificationReport rep = verify_solution(m, sol, opts);
  EXPECT_TRUE(rep.passed());
  EXPECT_EQ(rep.status, VerificationStatus::kVerifiedInfeasible);
  EXPECT_EQ(rep.certificate_check, CheckResult::kPass);
}

}  // namespace sankhya
