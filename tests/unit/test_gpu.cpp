// SPDX-License-Identifier: Apache-2.0
// SANKHYA - GPU foundation and CUDA runtime tests (Phase 2 Feature 1).
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

#include "sankhya/device.hpp"
#include "sankhya/gpu_ops.hpp"
#include "sankhya/gpu_runtime.hpp"
#include "sankhya/tolerances.hpp"

namespace sankhya {

// =========================================================================================
// 1. EXECUTION ABSTRACTION TESTS
// =========================================================================================

TEST(DeviceAbstraction, StringConversionAndParsing) {
  EXPECT_EQ(to_string(DeviceBackend::kCpu), "cpu");
  EXPECT_EQ(to_string(DeviceBackend::kGpu), "gpu");
  EXPECT_EQ(to_string(DeviceBackend::kHybrid), "hybrid");

  DeviceBackend b = DeviceBackend::kCpu;
  EXPECT_TRUE(parse_device_backend("cpu", &b));
  EXPECT_EQ(b, DeviceBackend::kCpu);

  EXPECT_TRUE(parse_device_backend("gpu", &b));
  EXPECT_EQ(b, DeviceBackend::kGpu);

  EXPECT_TRUE(parse_device_backend("cuda", &b));
  EXPECT_EQ(b, DeviceBackend::kGpu);

  EXPECT_TRUE(parse_device_backend("hybrid", &b));
  EXPECT_EQ(b, DeviceBackend::kHybrid);

  EXPECT_FALSE(parse_device_backend("quantum", &b));
}

TEST(DeviceAbstraction, ExecutionContextFromOptions) {
  Options opts_cpu;
  opts_cpu.set_bool("gpu", false);
  const ExecutionContext ctx_cpu = resolve_execution_context(opts_cpu);
  EXPECT_TRUE(ctx_cpu.is_cpu());
  EXPECT_FALSE(ctx_cpu.is_gpu());
  EXPECT_FALSE(ctx_cpu.is_hybrid());

  Options opts_gpu;
  opts_gpu.set_bool("gpu", true);
  const ExecutionContext ctx_gpu = resolve_execution_context(opts_gpu);
  EXPECT_FALSE(ctx_gpu.is_cpu());
  EXPECT_TRUE(ctx_gpu.is_gpu());
  EXPECT_FALSE(ctx_gpu.is_hybrid());
}

// =========================================================================================
// 2. RUNTIME AND DETECTION TESTS
// =========================================================================================

TEST(GpuRuntime, StatusStrings) {
  using gpu::GpuStatus;
  EXPECT_EQ(gpu::to_string(GpuStatus::kSuccess), "success");
  EXPECT_EQ(gpu::to_string(GpuStatus::kCudaUnavailable), "cuda unavailable");
  EXPECT_EQ(gpu::to_string(GpuStatus::kNoCompatibleDevice), "no compatible cuda device");
  EXPECT_EQ(gpu::to_string(GpuStatus::kOutOfMemory), "out of device memory");
  EXPECT_EQ(gpu::to_string(GpuStatus::kKernelLaunchFailed), "kernel launch failed");
  EXPECT_EQ(gpu::to_string(GpuStatus::kNonFiniteOutput), "non-finite numerical output detected");
}

TEST(GpuRuntime, AvailabilityProbeDoesNotCrash) {
  // Must return cleanly without raising signal, aborting, or crashing
  const bool avail = gpu::is_cuda_available();
  const int count = gpu::get_gpu_device_count();
  if (!avail) {
    EXPECT_EQ(count, 0);
  } else {
    EXPECT_GT(count, 0);
  }
}

// =========================================================================================
// 3. CPU REFERENCE IMPLEMENTATION TESTS
// =========================================================================================

TEST(GpuCpuReference, VectorAxpy) {
  const std::vector<double> x = {1.0, 2.0, -3.0, 4.5};
  std::vector<double> y = {0.5, -1.0, 2.0, 0.0};
  const double alpha = 2.0;

  gpu::cpu_axpy(static_cast<Index>(x.size()), alpha, x.data(), y.data());

  EXPECT_NEAR(y[0], 2.5, 1e-12);
  EXPECT_NEAR(y[1], 3.0, 1e-12);
  EXPECT_NEAR(y[2], -4.0, 1e-12);
  EXPECT_NEAR(y[3], 9.0, 1e-12);
}

TEST(GpuCpuReference, VectorDot) {
  const std::vector<double> x = {1.0, 2.0, -3.0, 4.0};
  const std::vector<double> y = {2.0, -1.0, 0.5, 3.0};
  // 1*2 + 2*(-1) + (-3)*0.5 + 4*3 = 2 - 2 - 1.5 + 12 = 10.5
  const double result = gpu::cpu_dot(static_cast<Index>(x.size()), x.data(), y.data());
  EXPECT_NEAR(result, 10.5, 1e-12);
}

TEST(GpuCpuReference, PdhgPrimalStepProjectionAndExtrapolation) {
  // 4 coordinates:
  // j=0: cost=1.0, at_y=0.0 -> grad=1.0. x=2.0, tau=0.5 -> unconstrained = 2.0 - 0.5*1.0 = 1.5.
  //      bounds [-1.0, 1.0]. Clamped to 1.0. Extrapolated = 2*1.0 - 2.0 = 0.0.
  // j=1: cost=-2.0, at_y=1.0 -> grad=-1.0. x=0.0, tau=0.5 -> uncl = 0.0 - 0.5*(-1) = 0.5.
  //      bounds [0.0, 2.0]. Clamped to 0.5. Extrapolated = 2*0.5 - 0.0 = 1.0.
  // j=2: free variable bounds [-inf, +inf]. uncl = -5.0. Clamped to -5.0.
  // j=3: lower-bounded only [0.0, +inf]. uncl = -1.0 -> clamped to 0.0.
  const Index n = 4;
  const double tau = 0.5;
  const std::vector<double> x = {2.0, 0.0, -4.0, 0.5};
  const std::vector<double> cost = {1.0, -2.0, 2.0, 3.0};
  const std::vector<double> at_y = {0.0, 1.0, 0.0, 0.0};
  const std::vector<double> col_lower = {-1.0, 0.0, -kInfinity, 0.0};
  const std::vector<double> col_upper = {1.0, 2.0, kInfinity, kInfinity};

  std::vector<double> x_next(n, 0.0);
  std::vector<double> extrapolated(n, 0.0);

  gpu::cpu_pdhg_primal_step(n, tau, x.data(), cost.data(), at_y.data(), col_lower.data(),
                            col_upper.data(), x_next.data(), extrapolated.data());

  EXPECT_NEAR(x_next[0], 1.0, 1e-12);
  EXPECT_NEAR(extrapolated[0], 0.0, 1e-12);

  EXPECT_NEAR(x_next[1], 0.5, 1e-12);
  EXPECT_NEAR(extrapolated[1], 1.0, 1e-12);

  EXPECT_NEAR(x_next[2], -5.0, 1e-12);
  EXPECT_NEAR(extrapolated[2], -6.0, 1e-12);

  EXPECT_NEAR(x_next[3], 0.0, 1e-12);
  EXPECT_NEAR(extrapolated[3], -0.5, 1e-12);
}

TEST(GpuCpuReference, SpmvCscBothOrientations) {
  // Matrix A (2 rows, 3 cols):
  // [ 1.0  0.0  2.0 ]
  // [ 0.0  3.0  4.0 ]
  // CSC format:
  // col 0: row 0 (val 1.0)
  // col 1: row 1 (val 3.0)
  // col 2: row 0 (val 2.0), row 1 (val 4.0)
  const Index num_rows = 2;
  const Index num_cols = 3;
  const std::vector<Index> col_starts = {0, 1, 2, 4};
  const std::vector<Index> row_indices = {0, 1, 0, 1};
  const std::vector<double> values = {1.0, 3.0, 2.0, 4.0};

  // Test y = A * x where x = [1, 2, 3]
  // y[0] = 1*1 + 0*2 + 2*3 = 7
  // y[1] = 0*1 + 3*2 + 4*3 = 18
  const std::vector<double> x_non_trans = {1.0, 2.0, 3.0};
  std::vector<double> y_non_trans(num_rows, 0.0);
  gpu::cpu_spmv_csc(num_rows, num_cols, col_starts.data(), row_indices.data(), values.data(),
                    x_non_trans.data(), y_non_trans.data(), 1.0, false);
  EXPECT_NEAR(y_non_trans[0], 7.0, 1e-12);
  EXPECT_NEAR(y_non_trans[1], 18.0, 1e-12);

  // Test y = A^T * x where x = [2, 5]
  // y[0] = 1*2 + 0*5 = 2
  // y[1] = 0*2 + 3*5 = 15
  // y[2] = 2*2 + 4*5 = 24
  const std::vector<double> x_trans = {2.0, 5.0};
  std::vector<double> y_trans(num_cols, 0.0);
  gpu::cpu_spmv_csc(num_rows, num_cols, col_starts.data(), row_indices.data(), values.data(),
                    x_trans.data(), y_trans.data(), 1.0, true);
  EXPECT_NEAR(y_trans[0], 2.0, 1e-12);
  EXPECT_NEAR(y_trans[1], 15.0, 1e-12);
  EXPECT_NEAR(y_trans[2], 24.0, 1e-12);
}

// =========================================================================================
// 4. NUMERICAL VALIDATION AND ERROR CHECKING TESTS
// =========================================================================================

TEST(GpuValidation, VectorComparisonPassing) {
  const std::vector<double> a = {1.0, 2.0, 3.0};
  const std::vector<double> b = {1.0, 2.00000001, 2.99999999};
  const auto res = gpu::compare_vectors(3, a.data(), b.data(), 1e-7);
  EXPECT_TRUE(res.passed);
  EXPECT_FALSE(res.contains_nan_or_inf);
  EXPECT_LE(res.max_abs_diff, 1e-7);
}

TEST(GpuValidation, VectorComparisonFailingOnLargeDifference) {
  const std::vector<double> a = {1.0, 2.0, 3.0};
  const std::vector<double> b = {1.0, 2.05, 3.0};
  const auto res = gpu::compare_vectors(3, a.data(), b.data(), 1e-4);
  EXPECT_FALSE(res.passed);
  EXPECT_NEAR(res.max_abs_diff, 0.05, 1e-6);
}

TEST(GpuValidation, VectorComparisonFailingOnNaN) {
  const double nan_val = std::numeric_limits<double>::quiet_NaN();
  const std::vector<double> a = {1.0, nan_val, 3.0};
  const std::vector<double> b = {1.0, 2.0, 3.0};
  const auto res = gpu::compare_vectors(3, a.data(), b.data(), 1e-4);
  EXPECT_FALSE(res.passed);
  EXPECT_TRUE(res.contains_nan_or_inf);
}

TEST(GpuValidation, VectorComparisonFailingOnInfinity) {
  const double inf_val = std::numeric_limits<double>::infinity();
  const std::vector<double> a = {1.0, inf_val, 3.0};
  const std::vector<double> b = {1.0, 2.0, 3.0};
  const auto res = gpu::compare_vectors(3, a.data(), b.data(), 1e-4);
  EXPECT_FALSE(res.passed);
  EXPECT_TRUE(res.contains_nan_or_inf);
}

// =========================================================================================
// 5. HIGH LEVEL DISPATCHER AND FAILURE SAFETY
// =========================================================================================

TEST(GpuDispatcher, SafeCpuExecutionWhenGpuNotPreferred) {
  const Index n = 2;
  const double tau = 0.1;
  const std::vector<double> x = {1.0, 2.0};
  const std::vector<double> cost = {0.5, -0.5};
  const std::vector<double> at_y = {0.0, 0.0};
  const std::vector<double> col_lower = {-10.0, -10.0};
  const std::vector<double> col_upper = {10.0, 10.0};

  std::vector<double> x_next(n);
  std::vector<double> extrapolated(n);

  gpu::NumericalComparisonResult cmp;
  const gpu::GpuStatus status = gpu::execute_pdhg_primal_step(
      n, tau, x.data(), cost.data(), at_y.data(), col_lower.data(), col_upper.data(),
      x_next.data(), extrapolated.data(), /*prefer_gpu=*/false, &cmp);

  // Even if CUDA is unavailable, CPU execution succeeds deterministically
  EXPECT_TRUE(status == gpu::GpuStatus::kSuccess || status == gpu::GpuStatus::kCudaUnavailable);
  EXPECT_NEAR(x_next[0], 0.95, 1e-12);
  EXPECT_NEAR(x_next[1], 2.05, 1e-12);
}

// =========================================================================================
// 6. HARDWARE-DEPENDENT CUDA TESTS (Safely skipped if no GPU present)
// =========================================================================================

TEST(GpuHardware, DeviceInfoAndKernelCorrectnessIfPresent) {
  if (!gpu::is_cuda_available()) {
    GTEST_SKIP() << "No CUDA device available in this environment; skipping hardware execution test";
  }

  // If CUDA is available, verify device properties
  gpu::GpuDeviceInfo info;
  const gpu::GpuStatus info_status = gpu::get_device_info(0, &info);
  EXPECT_EQ(info_status, gpu::GpuStatus::kSuccess);
  EXPECT_FALSE(info.name.empty());
  EXPECT_GT(info.total_memory_bytes, 0);

  // Real GPU execution test
  const Index n = 1024;
  std::vector<double> h_x(n, 1.5);
  std::vector<double> h_cost(n, 0.2);
  std::vector<double> h_at_y(n, -0.1);
  std::vector<double> h_lower(n, 0.0);
  std::vector<double> h_upper(n, 2.0);
  std::vector<double> gpu_x_next(n, 0.0);
  std::vector<double> gpu_extrapolated(n, 0.0);

  gpu::NumericalComparisonResult comparison;
  const gpu::GpuStatus exec_status = gpu::execute_pdhg_primal_step(
      n, 0.5, h_x.data(), h_cost.data(), h_at_y.data(), h_lower.data(), h_upper.data(),
      gpu_x_next.data(), gpu_extrapolated.data(), /*prefer_gpu=*/true, &comparison);

  EXPECT_EQ(exec_status, gpu::GpuStatus::kSuccess);
  EXPECT_TRUE(comparison.passed);
  EXPECT_LE(comparison.max_abs_diff, 1e-7);
}

}  // namespace sankhya
