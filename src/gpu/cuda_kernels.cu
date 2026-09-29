// SPDX-License-Identifier: Apache-2.0
// SANKHYA - CUDA computational kernels (Phase 2 Feature 1).
#ifdef SANKHYA_ENABLE_CUDA

#include <cuda_runtime.h>

#include <cmath>

#include "sankhya/gpu_ops.hpp"
#include "sankhya/gpu_runtime.hpp"

namespace sankhya::gpu {
namespace {

constexpr int kBlockDim = 256;

__device__ inline double dev_project_bound(double val, double lower, double upper) {
  // -1e30 is well within finite bounds limit
  if (lower > -1e30 && val < lower) return lower;
  if (upper < 1e30 && val > upper) return upper;
  return val;
}

// ---- Kernel 1: Vector AXPY -------------------------------------------------------------
__global__ void k_axpy(Index n, double alpha, const double* __restrict__ x,
                       double* __restrict__ y) {
  const int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx < n) {
    y[idx] += alpha * x[idx];
  }
}

// ---- Kernel 2: Parallel Reduction Dot Product ------------------------------------------
__global__ void k_dot_block(Index n, const double* __restrict__ x, const double* __restrict__ y,
                            double* __restrict__ block_out) {
  __shared__ double sdata[kBlockDim];
  const unsigned int tid = threadIdx.x;
  int i = blockIdx.x * (blockDim.x * 2) + threadIdx.x;

  double my_sum = 0.0;
  if (i < n) my_sum += x[i] * y[i];
  if (i + blockDim.x < n) my_sum += x[i + blockDim.x] * y[i + blockDim.x];

  sdata[tid] = my_sum;
  __syncthreads();

  for (unsigned int s = blockDim.x / 2; s > 0; s >>= 1) {
    if (tid < s) {
      sdata[tid] += sdata[tid + s];
    }
    __syncthreads();
  }

  if (tid == 0) {
    block_out[blockIdx.x] = sdata[0];
  }
}

// ---- Kernel 3: PDHG Primal Step (Projection + Extrapolation) ---------------------------
__global__ void k_pdhg_primal_step(Index n, double tau, const double* __restrict__ x,
                                   const double* __restrict__ cost,
                                   const double* __restrict__ at_y,
                                   const double* __restrict__ col_lower,
                                   const double* __restrict__ col_upper,
                                   double* __restrict__ x_next,
                                   double* __restrict__ extrapolated) {
  const int j = blockIdx.x * blockDim.x + threadIdx.x;
  if (j < n) {
    const double grad = cost[j] + at_y[j];
    const double uncl = x[j] - tau * grad;
    const double clamped = dev_project_bound(uncl, col_lower[j], col_upper[j]);
    x_next[j] = clamped;
    extrapolated[j] = 2.0 * clamped - x[j];
  }
}

// ---- Kernel 4: SpMV CSC Transpose (A^T * x) -------------------------------------------
__global__ void k_spmv_csc_transpose(Index num_cols, const Index* __restrict__ col_starts,
                                     const Index* __restrict__ row_indices,
                                     const double* __restrict__ values,
                                     const double* __restrict__ x, double* __restrict__ y,
                                     double alpha) {
  const int j = blockIdx.x * blockDim.x + threadIdx.x;
  if (j < num_cols) {
    const Index start = col_starts[j];
    const Index end = col_starts[j + 1];
    double dot = 0.0;
    for (Index p = start; p < end; ++p) {
      dot += values[p] * x[row_indices[p]];
    }
    y[j] = alpha * dot;
  }
}

// ---- Kernel 5: SpMV CSC Non-Transpose (A * x) -----------------------------------------
__global__ void k_spmv_csc_scatter(Index num_cols, const Index* __restrict__ col_starts,
                                   const Index* __restrict__ row_indices,
                                   const double* __restrict__ values,
                                   const double* __restrict__ x, double* __restrict__ y,
                                   double alpha) {
  const int j = blockIdx.x * blockDim.x + threadIdx.x;
  if (j < num_cols) {
    const double xj = alpha * x[j];
    if (xj != 0.0) {
      const Index start = col_starts[j];
      const Index end = col_starts[j + 1];
      for (Index p = start; p < end; ++p) {
        atomicAdd(&y[row_indices[p]], values[p] * xj);
      }
    }
  }
}

}  // namespace

GpuStatus launch_gpu_axpy(Index n, double alpha, const double* d_x, double* d_y) noexcept {
  if (n <= 0) return GpuStatus::kSuccess;
  if (!d_x || !d_y) return GpuStatus::kInvalidDeviceState;

  const int blocks = static_cast<int>((n + kBlockDim - 1) / kBlockDim);
  k_axpy<<<blocks, kBlockDim>>>(n, alpha, d_x, d_y);
  const cudaError_t err = cudaGetLastError();
  return (err == cudaSuccess) ? GpuStatus::kSuccess : GpuStatus::kKernelLaunchFailed;
}

GpuStatus launch_gpu_dot(Index n, const double* d_x, const double* d_y,
                         double* host_result) noexcept {
  if (n <= 0 || !host_result) return GpuStatus::kInvalidDeviceState;
  if (!d_x || !d_y) return GpuStatus::kInvalidDeviceState;

  const int blocks = static_cast<int>((n + (kBlockDim * 2) - 1) / (kBlockDim * 2));
  double* d_blocks = nullptr;
  cudaError_t err = cudaMalloc(&d_blocks, static_cast<std::size_t>(blocks) * sizeof(double));
  if (err != cudaSuccess) return GpuStatus::kOutOfMemory;

  k_dot_block<<<blocks, kBlockDim>>>(n, d_x, d_y, d_blocks);
  err = cudaGetLastError();
  if (err != cudaSuccess) {
    cudaFree(d_blocks);
    return GpuStatus::kKernelLaunchFailed;
  }

  std::vector<double> h_blocks(static_cast<std::size_t>(blocks));
  err = cudaMemcpy(h_blocks.data(), d_blocks, static_cast<std::size_t>(blocks) * sizeof(double),
                   cudaMemcpyDeviceToHost);
  cudaFree(d_blocks);
  if (err != cudaSuccess) return GpuStatus::kCopyFailed;

  double total = 0.0;
  for (int b = 0; b < blocks; ++b) {
    total += h_blocks[static_cast<std::size_t>(b)];
  }
  *host_result = total;
  return GpuStatus::kSuccess;
}

GpuStatus launch_gpu_pdhg_primal_step(Index n, double tau, const double* d_x,
                                      const double* d_cost, const double* d_at_y,
                                      const double* d_col_lower,
                                      const double* d_col_upper, double* d_x_next,
                                      double* d_extrapolated) noexcept {
  if (n <= 0) return GpuStatus::kSuccess;
  if (!d_x || !d_cost || !d_at_y || !d_col_lower || !d_col_upper || !d_x_next || !d_extrapolated) {
    return GpuStatus::kInvalidDeviceState;
  }

  const int blocks = static_cast<int>((n + kBlockDim - 1) / kBlockDim);
  k_pdhg_primal_step<<<blocks, kBlockDim>>>(n, tau, d_x, d_cost, d_at_y, d_col_lower, d_col_upper,
                                           d_x_next, d_extrapolated);
  const cudaError_t err = cudaGetLastError();
  return (err == cudaSuccess) ? GpuStatus::kSuccess : GpuStatus::kKernelLaunchFailed;
}

GpuStatus launch_gpu_spmv_csc(Index num_rows, Index num_cols, const Index* d_col_starts,
                              const Index* d_row_indices, const double* d_values,
                              const double* d_x, double* d_y, double alpha,
                              bool transpose) noexcept {
  if (num_rows <= 0 || num_cols <= 0 || !d_col_starts || !d_row_indices || !d_values || !d_x ||
      !d_y) {
    return GpuStatus::kInvalidDeviceState;
  }

  if (transpose) {
    const int blocks = static_cast<int>((num_cols + kBlockDim - 1) / kBlockDim);
    k_spmv_csc_transpose<<<blocks, kBlockDim>>>(num_cols, d_col_starts, d_row_indices, d_values,
                                                d_x, d_y, alpha);
  } else {
    cudaMemset(d_y, 0, static_cast<std::size_t>(num_rows) * sizeof(double));
    const int blocks = static_cast<int>((num_cols + kBlockDim - 1) / kBlockDim);
    k_spmv_csc_scatter<<<blocks, kBlockDim>>>(num_cols, d_col_starts, d_row_indices, d_values,
                                             d_x, d_y, alpha);
  }

  const cudaError_t err = cudaGetLastError();
  return (err == cudaSuccess) ? GpuStatus::kSuccess : GpuStatus::kKernelLaunchFailed;
}

}  // namespace sankhya::gpu

#endif  // SANKHYA_ENABLE_CUDA
