// SPDX-License-Identifier: Apache-2.0
// SANKHYA - GPU operations and CPU reference paths (Phase 2 Feature 1).
//
// Defines:
//  - GPU computational kernel entry points (real CUDA execution)
//  - Deterministic CPU reference counterparts
//  - Numerical validation and error checking functions
#pragma once

#include <cstddef>
#include <vector>

#include "sankhya/gpu_runtime.hpp"
#include "sankhya/tolerances.hpp"
#include "sankhya/types.hpp"

namespace sankhya::gpu {

// =========================================================================================
// CPU REFERENCE IMPLEMENTATIONS (Baseline Truth)
// =========================================================================================

/// CPU reference: y[i] += alpha * x[i]
void cpu_axpy(Index n, double alpha, const double* x, double* y) noexcept;

/// CPU reference: dot product sum_i (x[i] * y[i])
[[nodiscard]] double cpu_dot(Index n, const double* x, const double* y) noexcept;

/// CPU reference: PDHG Primal coordinate update + box projection + Chambolle-Pock extrapolation
/// For each coordinate j:
///   grad_j = cost[j] + at_y[j]
///   x_next[j] = project(x[j] - tau * grad_j, col_lower[j], col_upper[j])
///   extrapolated[j] = 2.0 * x_next[j] - x[j]
void cpu_pdhg_primal_step(Index n, double tau, const double* x, const double* cost,
                          const double* at_y, const double* col_lower, const double* col_upper,
                          double* x_next, double* extrapolated) noexcept;

/// CPU reference: Sparse matrix-vector product for CSC matrix.
/// If transpose == false: y = alpha * A * x (y has num_rows elements)
/// If transpose == true:  y = alpha * A^T * x (y has num_cols elements)
void cpu_spmv_csc(Index num_rows, Index num_cols, const Index* col_starts, const Index* row_indices,
                  const double* values, const double* x, double* y, double alpha,
                  bool transpose) noexcept;

// =========================================================================================
// REAL GPU KERNEL CALLS (Executed on Device via CUDA)
// =========================================================================================

/// GPU kernel: y[i] += alpha * x[i]
[[nodiscard]] GpuStatus launch_gpu_axpy(Index n, double alpha, const double* d_x,
                                        double* d_y) noexcept;

/// GPU kernel: parallel reduction dot product
[[nodiscard]] GpuStatus launch_gpu_dot(Index n, const double* d_x, const double* d_y,
                                       double* host_result) noexcept;

/// GPU kernel: PDHG Primal step coordinate-wise parallel projection and extrapolation
[[nodiscard]] GpuStatus launch_gpu_pdhg_primal_step(
    Index n, double tau, const double* d_x, const double* d_cost, const double* d_at_y,
    const double* d_col_lower, const double* d_col_upper, double* d_x_next,
    double* d_extrapolated) noexcept;

/// GPU kernel: Sparse matrix-vector product on device memory
[[nodiscard]] GpuStatus launch_gpu_spmv_csc(
    Index num_rows, Index num_cols, const Index* d_col_starts, const Index* d_row_indices,
    const double* d_values, const double* d_x, double* d_y, double alpha,
    bool transpose) noexcept;

// =========================================================================================
// HIGH-LEVEL VALIDATED DISPATCHERS (CPU <-> GPU Comparison & Validation)
// =========================================================================================

struct NumericalComparisonResult {
  bool passed = false;
  double max_abs_diff = 0.0;
  double max_rel_diff = 0.0;
  bool contains_nan_or_inf = false;
};

/// Compare GPU result array against CPU reference array within numerical tolerance.
[[nodiscard]] NumericalComparisonResult compare_vectors(
    Index n, const double* test_vec, const double* ref_vec,
    double tol = tol::kPrimalFeasibility) noexcept;

/// Execute PDHG primal step with automatic fallback and validation:
/// If GPU is available and requested, runs on GPU and validates.
/// If GPU fails or is unavailable, executes safely on CPU without corruption.
[[nodiscard]] GpuStatus execute_pdhg_primal_step(
    Index n, double tau, const double* x, const double* cost, const double* at_y,
    const double* col_lower, const double* col_upper, double* x_next,
    double* extrapolated, bool prefer_gpu, NumericalComparisonResult* opt_comparison = nullptr);

}  // namespace sankhya::gpu
