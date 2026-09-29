// SPDX-License-Identifier: Apache-2.0
// SANKHYA - Adaptive backend selection tests (Phase 2 Feature 2).
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "sankhya/adaptive.hpp"
#include "sankhya/gpu_runtime.hpp"
#include "sankhya/model.hpp"
#include "sankhya/options.hpp"

namespace sankhya {
namespace {

Model make_synthetic_model(Index rows, Index cols, Index nnz_per_col, bool integer = false) {
  Model m;
  m.name = "synthetic";
  m.sense = ObjSense::kMinimize;
  m.row_lower.assign(static_cast<std::size_t>(rows), 0.0);
  m.row_upper.assign(static_cast<std::size_t>(rows), 10.0);
  m.col_lower.assign(static_cast<std::size_t>(cols), 0.0);
  m.col_upper.assign(static_cast<std::size_t>(cols), 1.0);
  m.cost.assign(static_cast<std::size_t>(cols), 1.0);
  m.is_integer.assign(static_cast<std::size_t>(cols), integer);

  m.matrix.reset(rows, cols);
  for (Index j = 0; j < cols; ++j) {
    for (Index k = 0; k < nnz_per_col; ++k) {
      const Index r = (j + k) % rows;
      m.matrix.add_entry(r, j, 1.0);
    }
  }
  m.matrix.finalize();
  return m;
}

}  // namespace

// =========================================================================================
// 1. PROBLEM ANALYZER TESTS
// =========================================================================================

TEST(AdaptiveAnalyzer, MeasuresModelDimensionsAndCharacteristics) {
  const Model m = make_synthetic_model(10, 20, 3, /*integer=*/false);
  const ProblemCharacteristics chars = analyze_problem(m);

  EXPECT_EQ(chars.num_rows, 10);
  EXPECT_EQ(chars.num_cols, 20);
  EXPECT_EQ(chars.num_nonzeros, 60);
  EXPECT_EQ(chars.num_integer_cols, 0);
  EXPECT_EQ(chars.problem_class_name, "LP");
  EXPECT_NEAR(chars.density, 60.0 / 200.0, 1e-6);
  EXPECT_NEAR(chars.sparsity_ratio, 1.0 - (60.0 / 200.0), 1e-6);
  EXPECT_GT(chars.estimated_matvec_flops, 0.0);
  EXPECT_GT(chars.estimated_memory_bytes, 0.0);
}

TEST(AdaptiveAnalyzer, ClassifiesMilpCorrectly) {
  const Model m = make_synthetic_model(5, 5, 2, /*integer=*/true);
  const ProblemCharacteristics chars = analyze_problem(m);
  EXPECT_EQ(chars.problem_class_name, "MILP");
  EXPECT_EQ(chars.num_integer_cols, 5);
}

// =========================================================================================
// 2. BACKEND SELECTION: EXPLICIT MODES (TASK 5)
// =========================================================================================

TEST(AdaptiveSelector, ExplicitCpuForcesCpu) {
  const Model m = make_synthetic_model(2000, 2000, 30);  // large 60,000 nnz
  const ProblemCharacteristics chars = analyze_problem(m);

  Options opts;
  opts.set_string("backend", "cpu");
  AdaptiveConfig cfg;

  const BackendDecision dec = select_backend(chars, opts, cfg);
  EXPECT_EQ(dec.selected_backend, DeviceBackend::kCpu);
  EXPECT_EQ(dec.requested_mode, BackendMode::kCpu);
  EXPECT_TRUE(dec.is_valid);
  EXPECT_NE(dec.primary_reason.find("Explicitly requested CPU"), std::string::npos);
}

TEST(AdaptiveSelector, ExplicitGpuFailsClearlyWhenGpuUnavailable) {
  // Simulate an environment with no GPU
  ProblemCharacteristics chars;
  chars.num_rows = 5000;
  chars.num_cols = 5000;
  chars.num_nonzeros = 100000;
  chars.problem_class_name = "LP";
  chars.gpu_available = false;

  Options opts;
  opts.set_string("backend", "gpu");
  AdaptiveConfig cfg;

  const BackendDecision dec = select_backend(chars, opts, cfg);
  // Must NOT silently fall back: explicit GPU request must fail clearly
  EXPECT_EQ(dec.requested_mode, BackendMode::kGpu);
  EXPECT_FALSE(dec.is_valid);
  EXPECT_NE(dec.primary_reason.find("CUDA device is not available"), std::string::npos);
}

TEST(AdaptiveSelector, ExplicitHybridFailsClearlyAsUnsupported) {
  ProblemCharacteristics chars;
  chars.num_rows = 100;
  chars.num_cols = 100;
  chars.num_nonzeros = 500;
  chars.problem_class_name = "LP";

  Options opts;
  opts.set_string("backend", "hybrid");
  AdaptiveConfig cfg;

  const BackendDecision dec = select_backend(chars, opts, cfg);
  // Never falsely route to unimplemented hybrid
  EXPECT_EQ(dec.selected_backend, DeviceBackend::kHybrid);
  EXPECT_EQ(dec.requested_mode, BackendMode::kHybrid);
  EXPECT_FALSE(dec.is_valid);
  EXPECT_NE(dec.primary_reason.find("unsupported in this version"), std::string::npos);
}

// =========================================================================================
// 3. BACKEND SELECTION: AUTO ADAPTIVE DECISIONS (TASK 3, 4, 6)
// =========================================================================================

TEST(AdaptiveSelector, AutoSelectsCpuWhenGpuUnavailable) {
  ProblemCharacteristics chars;
  chars.num_rows = 10000;
  chars.num_cols = 10000;
  chars.num_nonzeros = 200000;
  chars.problem_class_name = "LP";
  chars.gpu_available = false;

  Options opts;
  opts.set_string("backend", "auto");
  AdaptiveConfig cfg;

  const BackendDecision dec = select_backend(chars, opts, cfg);
  // Under AUTO, safe fallback to CPU is expected and valid
  EXPECT_EQ(dec.selected_backend, DeviceBackend::kCpu);
  EXPECT_EQ(dec.requested_mode, BackendMode::kAuto);
  EXPECT_TRUE(dec.is_valid);
  EXPECT_NE(dec.primary_reason.find("CUDA GPU is not available"), std::string::npos);
}

TEST(AdaptiveSelector, AutoSelectsCpuForSmallProblemEvenIfGpuAvailable) {
  // Small problem: 100 rows, 100 cols, 300 nonzeros
  ProblemCharacteristics chars;
  chars.num_rows = 100;
  chars.num_cols = 100;
  chars.num_nonzeros = 300;
  chars.problem_class_name = "LP";
  chars.gpu_available = true;
  chars.gpu_name = "NVIDIA RTX Test";
  chars.gpu_memory_bytes = 8ULL * 1024 * 1024 * 1024;

  Options opts;
  opts.set_string("backend", "auto");
  AdaptiveConfig cfg;
  cfg.min_gpu_nonzeros = 50000;
  cfg.min_gpu_dimension = 1000;

  const BackendDecision dec = select_backend(chars, opts, cfg);
  // Must choose CPU to avoid PCIe transfer overhead on small problem
  EXPECT_EQ(dec.selected_backend, DeviceBackend::kCpu);
  EXPECT_TRUE(dec.is_valid);
  EXPECT_NE(dec.primary_reason.find("below GPU threshold"), std::string::npos);
}

TEST(AdaptiveSelector, AutoSelectsCpuForMilpEvenIfGpuAvailable) {
  ProblemCharacteristics chars;
  chars.num_rows = 5000;
  chars.num_cols = 5000;
  chars.num_nonzeros = 100000;
  chars.num_integer_cols = 200;
  chars.problem_class_name = "MILP";
  chars.gpu_available = true;
  chars.gpu_name = "NVIDIA RTX Test";
  chars.gpu_memory_bytes = 8ULL * 1024 * 1024 * 1024;

  Options opts;
  opts.set_string("backend", "auto");
  AdaptiveConfig cfg;

  const BackendDecision dec = select_backend(chars, opts, cfg);
  EXPECT_EQ(dec.selected_backend, DeviceBackend::kCpu);
  EXPECT_TRUE(dec.is_valid);
  EXPECT_NE(dec.primary_reason.find("MILP"), std::string::npos);
}

TEST(AdaptiveSelector, AutoSelectsGpuForLargeLpWhenGpuAvailable) {
  // Large problem: 2,000 rows, 2,000 cols, 80,000 nonzeros
  ProblemCharacteristics chars;
  chars.num_rows = 2000;
  chars.num_cols = 2000;
  chars.num_nonzeros = 80000;
  chars.problem_class_name = "LP";
  chars.gpu_available = true;
  chars.gpu_name = "NVIDIA RTX Test";
  chars.gpu_memory_bytes = 8ULL * 1024 * 1024 * 1024;

  Options opts;
  opts.set_string("backend", "auto");
  AdaptiveConfig cfg;
  cfg.min_gpu_nonzeros = 50000;
  cfg.min_gpu_dimension = 1000;

  const BackendDecision dec = select_backend(chars, opts, cfg);
  EXPECT_EQ(dec.selected_backend, DeviceBackend::kGpu);
  EXPECT_TRUE(dec.is_valid);
  EXPECT_NE(dec.primary_reason.find("exceeds GPU acceleration threshold"), std::string::npos);
}

TEST(AdaptiveSelector, DecisionIsDeterministic) {
  const Model m = make_synthetic_model(50, 50, 4);
  const ProblemCharacteristics chars = analyze_problem(m);
  Options opts;
  AdaptiveConfig cfg;

  const BackendDecision dec1 = select_backend(chars, opts, cfg);
  const BackendDecision dec2 = select_backend(chars, opts, cfg);

  EXPECT_EQ(dec1.selected_backend, dec2.selected_backend);
  EXPECT_EQ(dec1.requested_mode, dec2.requested_mode);
  EXPECT_EQ(dec1.is_valid, dec2.is_valid);
  EXPECT_EQ(dec1.primary_reason, dec2.primary_reason);
}

TEST(AdaptiveSelector, DecisionExplanationMatchesSelectedBackend) {
  ProblemCharacteristics chars;
  chars.num_rows = 50;
  chars.num_cols = 50;
  chars.num_nonzeros = 150;
  chars.problem_class_name = "LP";
  chars.gpu_available = false;

  Options opts;
  opts.set_string("backend", "auto");
  AdaptiveConfig cfg;

  const BackendDecision dec = select_backend(chars, opts, cfg);
  const std::string explanation = dec.format_explanation();

  EXPECT_NE(explanation.find("Backend decision: cpu"), std::string::npos);
  EXPECT_NE(explanation.find("Primary reason:"), std::string::npos);
  EXPECT_NE(explanation.find("Detailed factors:"), std::string::npos);
}

// =========================================================================================
// 4. SOLVER INTEGRATION TEST WITH EXPLICIT BACKEND FAILURES
// =========================================================================================

TEST(AdaptiveSolverIntegration, ExplicitGpuRejectsSolveWhenGpuUnavailable) {
  // If running on a system with no CUDA GPU, --backend=gpu must refuse the solve cleanly
  if (gpu::is_cuda_available()) {
    GTEST_SKIP() << "CUDA GPU is available; skipping unavailability rejection test";
  }

  const Model model = make_synthetic_model(4, 4, 2);
  Options opts;
  opts.set_string("backend", "gpu");

  const Solution sol = solve(model, opts);
  EXPECT_EQ(sol.status, SolveStatus::kNotSolved);
  EXPECT_NE(sol.message.find("CUDA device is not available"), std::string::npos);
}

TEST(AdaptiveSolverIntegration, AutoModeSolvesCleanlyOnCpuWhenGpuUnavailable) {
  const Model model = make_synthetic_model(4, 4, 2);
  Options opts;
  opts.set_string("backend", "auto");

  const Solution sol = solve(model, opts);
  EXPECT_EQ(sol.status, SolveStatus::kOptimal);
}

}  // namespace sankhya
