// SPDX-License-Identifier: Apache-2.0
// SANKHYA - GPU runtime stub for CPU-only builds (Phase 2 Feature 1).
//
// Used when SANKHYA_ENABLE_CUDA is OFF or CUDA is absent. Provides clean, safe
// fallbacks with explicit GpuStatus::kCudaUnavailable without crashing or faking execution.
#include "sankhya/gpu_ops.hpp"
#include "sankhya/gpu_runtime.hpp"

namespace sankhya::gpu {

bool is_cuda_available() noexcept {
  return false;
}

int get_gpu_device_count() noexcept {
  return 0;
}

GpuStatus get_device_info(int /*device_id*/, GpuDeviceInfo* /*out_info*/) noexcept {
  return GpuStatus::kCudaUnavailable;
}

GpuStatus set_device(int /*device_id*/) noexcept {
  return GpuStatus::kCudaUnavailable;
}

GpuStatus synchronize_device() noexcept {
  return GpuStatus::kCudaUnavailable;
}

std::string get_last_error_string() noexcept {
  return "CUDA is not enabled in this build";
}

GpuStatus allocate_device_memory(void** dev_ptr, std::size_t /*bytes*/) noexcept {
  if (dev_ptr) *dev_ptr = nullptr;
  return GpuStatus::kCudaUnavailable;
}

GpuStatus free_device_memory(void* /*dev_ptr*/) noexcept {
  return GpuStatus::kCudaUnavailable;
}

GpuStatus copy_host_to_device(void* /*dev_dst*/, const void* /*host_src*/,
                              std::size_t /*bytes*/) noexcept {
  return GpuStatus::kCudaUnavailable;
}

GpuStatus copy_device_to_host(void* /*host_dst*/, const void* /*dev_src*/,
                              std::size_t /*bytes*/) noexcept {
  return GpuStatus::kCudaUnavailable;
}

GpuStatus memset_device(void* /*dev_ptr*/, int /*value*/, std::size_t /*bytes*/) noexcept {
  return GpuStatus::kCudaUnavailable;
}

GpuStatus launch_gpu_axpy(Index /*n*/, double /*alpha*/, const double* /*d_x*/,
                          double* /*d_y*/) noexcept {
  return GpuStatus::kCudaUnavailable;
}

GpuStatus launch_gpu_dot(Index /*n*/, const double* /*d_x*/, const double* /*d_y*/,
                         double* /*host_result*/) noexcept {
  return GpuStatus::kCudaUnavailable;
}

GpuStatus launch_gpu_pdhg_primal_step(Index /*n*/, double /*tau*/, const double* /*d_x*/,
                                      const double* /*d_cost*/, const double* /*d_at_y*/,
                                      const double* /*d_col_lower*/,
                                      const double* /*d_col_upper*/, double* /*d_x_next*/,
                                      double* /*d_extrapolated*/) noexcept {
  return GpuStatus::kCudaUnavailable;
}

GpuStatus launch_gpu_spmv_csc(Index /*num_rows*/, Index /*num_cols*/,
                              const Index* /*d_col_starts*/, const Index* /*d_row_indices*/,
                              const double* /*d_values*/, const double* /*d_x*/,
                              double* /*d_y*/, double /*alpha*/,
                              bool /*transpose*/) noexcept {
  return GpuStatus::kCudaUnavailable;
}

}  // namespace sankhya::gpu
