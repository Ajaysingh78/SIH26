// SPDX-License-Identifier: Apache-2.0
// SANKHYA - CPU reference implementations and numerical validators (Phase 2 Feature 1).
#include "sankhya/gpu_ops.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace sankhya::gpu {
namespace {

inline double project_bound(double val, double lower, double upper) noexcept {
  if (is_finite_bound(lower) && val < lower) return lower;
  if (is_finite_bound(upper) && val > upper) return upper;
  return val;
}

}  // namespace

void cpu_axpy(Index n, double alpha, const double* x, double* y) noexcept {
  if (n <= 0 || !x || !y) return;
  for (Index i = 0; i < n; ++i) {
    y[static_cast<std::size_t>(i)] += alpha * x[static_cast<std::size_t>(i)];
  }
}

double cpu_dot(Index n, const double* x, const double* y) noexcept {
  if (n <= 0 || !x || !y) return 0.0;
  double sum = 0.0;
  for (Index i = 0; i < n; ++i) {
    sum += x[static_cast<std::size_t>(i)] * y[static_cast<std::size_t>(i)];
  }
  return sum;
}

void cpu_pdhg_primal_step(Index n, double tau, const double* x, const double* cost,
                          const double* at_y, const double* col_lower, const double* col_upper,
                          double* x_next, double* extrapolated) noexcept {
  if (n <= 0) return;
  for (Index j = 0; j < n; ++j) {
    const auto u = static_cast<std::size_t>(j);
    const double grad = cost[u] + at_y[u];
    const double unconstrained = x[u] - tau * grad;
    const double clamped = project_bound(unconstrained, col_lower[u], col_upper[u]);
    x_next[u] = clamped;
    extrapolated[u] = 2.0 * clamped - x[u];
  }
}

void cpu_spmv_csc(Index num_rows, Index num_cols, const Index* col_starts, const Index* row_indices,
                  const double* values, const double* x, double* y, double alpha,
                  bool transpose) noexcept {
  if (num_rows <= 0 || num_cols <= 0 || !col_starts || !row_indices || !values || !x || !y) {
    return;
  }
  if (!transpose) {
    // y = alpha * A * x. y has num_rows entries. Initialize y to 0.
    std::fill(y, y + num_rows, 0.0);
    for (Index j = 0; j < num_cols; ++j) {
      const double xj = alpha * x[static_cast<std::size_t>(j)];
      if (xj == 0.0) continue;
      const Index start = col_starts[static_cast<std::size_t>(j)];
      const Index end = col_starts[static_cast<std::size_t>(j + 1)];
      for (Index p = start; p < end; ++p) {
        const auto u_row = static_cast<std::size_t>(row_indices[static_cast<std::size_t>(p)]);
        y[u_row] += values[static_cast<std::size_t>(p)] * xj;
      }
    }
  } else {
    // y = alpha * A^T * x. y has num_cols entries.
    for (Index j = 0; j < num_cols; ++j) {
      const Index start = col_starts[static_cast<std::size_t>(j)];
      const Index end = col_starts[static_cast<std::size_t>(j + 1)];
      double dot = 0.0;
      for (Index p = start; p < end; ++p) {
        const auto u_row = static_cast<std::size_t>(row_indices[static_cast<std::size_t>(p)]);
        dot += values[static_cast<std::size_t>(p)] * x[u_row];
      }
      y[static_cast<std::size_t>(j)] = alpha * dot;
    }
  }
}

NumericalComparisonResult compare_vectors(Index n, const double* test_vec, const double* ref_vec,
                                          double tol) noexcept {
  NumericalComparisonResult result;
  result.passed = true;
  if (n <= 0) return result;
  if (!test_vec || !ref_vec) {
    result.passed = false;
    return result;
  }

  double max_abs = 0.0;
  double max_rel = 0.0;

  for (Index i = 0; i < n; ++i) {
    const auto u = static_cast<std::size_t>(i);
    const double val_test = test_vec[u];
    const double val_ref = ref_vec[u];

    if (!std::isfinite(val_test) || !std::isfinite(val_ref)) {
      result.contains_nan_or_inf = true;
      result.passed = false;
      return result;
    }

    const double abs_diff = std::fabs(val_test - val_ref);
    if (abs_diff > max_abs) max_abs = abs_diff;

    const double denom = std::max(1.0, std::max(std::fabs(val_test), std::fabs(val_ref)));
    const double rel_diff = abs_diff / denom;
    if (rel_diff > max_rel) max_rel = rel_diff;

    if (abs_diff > tol && rel_diff > tol) {
      result.passed = false;
    }
  }

  result.max_abs_diff = max_abs;
  result.max_rel_diff = max_rel;
  return result;
}

GpuStatus execute_pdhg_primal_step(Index n, double tau, const double* x, const double* cost,
                                   const double* at_y, const double* col_lower,
                                   const double* col_upper, double* x_next, double* extrapolated,
                                   bool prefer_gpu, NumericalComparisonResult* opt_comparison) {
  if (n <= 0) return GpuStatus::kSuccess;

  // Always compute CPU reference if comparison is requested or if CPU is preferred
  if (!prefer_gpu || !is_cuda_available()) {
    cpu_pdhg_primal_step(n, tau, x, cost, at_y, col_lower, col_upper, x_next, extrapolated);
    return is_cuda_available() ? GpuStatus::kSuccess : GpuStatus::kCudaUnavailable;
  }

  // GPU execution path
  DeviceBuffer<double> d_x(static_cast<std::size_t>(n));
  DeviceBuffer<double> d_cost(static_cast<std::size_t>(n));
  DeviceBuffer<double> d_at_y(static_cast<std::size_t>(n));
  DeviceBuffer<double> d_lower(static_cast<std::size_t>(n));
  DeviceBuffer<double> d_upper(static_cast<std::size_t>(n));
  DeviceBuffer<double> d_x_next(static_cast<std::size_t>(n));
  DeviceBuffer<double> d_extrapolated(static_cast<std::size_t>(n));

  const std::size_t un = static_cast<std::size_t>(n);
  GpuStatus status = d_x.upload(x, un);
  if (status == GpuStatus::kSuccess) status = d_cost.upload(cost, un);
  if (status == GpuStatus::kSuccess) status = d_at_y.upload(at_y, un);
  if (status == GpuStatus::kSuccess) status = d_lower.upload(col_lower, un);
  if (status == GpuStatus::kSuccess) status = d_upper.upload(col_upper, un);

  if (status != GpuStatus::kSuccess) {
    // Allocation or copy failure: safely fallback to CPU
    cpu_pdhg_primal_step(n, tau, x, cost, at_y, col_lower, col_upper, x_next, extrapolated);
    return status;
  }

  status = launch_gpu_pdhg_primal_step(n, tau, d_x.data(), d_cost.data(), d_at_y.data(),
                                       d_lower.data(), d_upper.data(), d_x_next.data(),
                                       d_extrapolated.data());
  if (status != GpuStatus::kSuccess) {
    // Kernel launch failure: safely fallback to CPU
    cpu_pdhg_primal_step(n, tau, x, cost, at_y, col_lower, col_upper, x_next, extrapolated);
    return status;
  }

  status = synchronize_device();
  if (status != GpuStatus::kSuccess) {
    cpu_pdhg_primal_step(n, tau, x, cost, at_y, col_lower, col_upper, x_next, extrapolated);
    return status;
  }

  status = d_x_next.download(x_next, un);
  if (status == GpuStatus::kSuccess) {
    status = d_extrapolated.download(extrapolated, un);
  }
  if (status != GpuStatus::kSuccess) {
    cpu_pdhg_primal_step(n, tau, x, cost, at_y, col_lower, col_upper, x_next, extrapolated);
    return status;
  }

  // Numerical sanity check on GPU output: ensure no NaNs or Infs leaked
  for (std::size_t i = 0; i < un; ++i) {
    if (!std::isfinite(x_next[i]) || !std::isfinite(extrapolated[i])) {
      // Revert to CPU on non-finite output
      cpu_pdhg_primal_step(n, tau, x, cost, at_y, col_lower, col_upper, x_next, extrapolated);
      return GpuStatus::kNonFiniteOutput;
    }
  }

  if (opt_comparison) {
    std::vector<double> ref_x_next(un);
    std::vector<double> ref_extrapolated(un);
    cpu_pdhg_primal_step(n, tau, x, cost, at_y, col_lower, col_upper, ref_x_next.data(),
                         ref_extrapolated.data());
    const auto cmp_x = compare_vectors(n, x_next, ref_x_next.data());
    const auto cmp_e = compare_vectors(n, extrapolated, ref_extrapolated.data());
    opt_comparison->passed = cmp_x.passed && cmp_e.passed;
    opt_comparison->max_abs_diff = std::max(cmp_x.max_abs_diff, cmp_e.max_abs_diff);
    opt_comparison->max_rel_diff = std::max(cmp_x.max_rel_diff, cmp_e.max_rel_diff);
    opt_comparison->contains_nan_or_inf = cmp_x.contains_nan_or_inf || cmp_e.contains_nan_or_inf;
  }

  return GpuStatus::kSuccess;
}

}  // namespace sankhya::gpu
