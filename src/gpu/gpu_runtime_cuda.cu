// SPDX-License-Identifier: Apache-2.0
// SANKHYA - CUDA runtime implementation (Phase 2 Feature 1).
#ifdef SANKHYA_ENABLE_CUDA

#include <cuda_runtime.h>

#include <cstring>
#include <string>

#include "sankhya/gpu_runtime.hpp"

namespace sankhya::gpu {
namespace {

GpuStatus map_cuda_error(cudaError_t err) noexcept {
  switch (err) {
    case cudaSuccess:
      return GpuStatus::kSuccess;
    case cudaErrorMemoryAllocation:
      return GpuStatus::kOutOfMemory;
    case cudaErrorInvalidValue:
    case cudaErrorInvalidDevicePointer:
      return GpuStatus::kInvalidDeviceState;
    case cudaErrorLaunchFailure:
    case cudaErrorLaunchOutOfResources:
      return GpuStatus::kKernelLaunchFailed;
    case cudaErrorNoDevice:
      return GpuStatus::kNoCompatibleDevice;
    case cudaErrorInitializationError:
      return GpuStatus::kInitFailed;
    default:
      return GpuStatus::kSyncFailed;
  }
}

}  // namespace

bool is_cuda_available() noexcept {
  int count = 0;
  const cudaError_t err = cudaGetDeviceCount(&count);
  return (err == cudaSuccess && count > 0);
}

int get_gpu_device_count() noexcept {
  int count = 0;
  const cudaError_t err = cudaGetDeviceCount(&count);
  if (err != cudaSuccess) return 0;
  return count;
}

GpuStatus get_device_info(int device_id, GpuDeviceInfo* out_info) noexcept {
  if (!out_info) return GpuStatus::kInvalidDeviceState;
  cudaDeviceProp prop;
  const cudaError_t err = cudaGetDeviceProperties(&prop, device_id);
  if (err != cudaSuccess) return map_cuda_error(err);

  out_info->device_id = device_id;
  out_info->name = prop.name;
  out_info->total_memory_bytes = prop.totalGlobalMem;
  out_info->compute_capability_major = prop.major;
  out_info->compute_capability_minor = prop.minor;
  out_info->multiprocessor_count = prop.multiProcessorCount;
  out_info->warp_size = prop.warpSize;
  out_info->max_threads_per_block = prop.maxThreadsPerBlock;
  return GpuStatus::kSuccess;
}

GpuStatus set_device(int device_id) noexcept {
  const cudaError_t err = cudaSetDevice(device_id);
  return map_cuda_error(err);
}

GpuStatus synchronize_device() noexcept {
  const cudaError_t err = cudaDeviceSynchronize();
  return map_cuda_error(err);
}

std::string get_last_error_string() noexcept {
  return cudaGetErrorString(cudaGetLastError());
}

GpuStatus allocate_device_memory(void** dev_ptr, std::size_t bytes) noexcept {
  if (!dev_ptr) return GpuStatus::kInvalidDeviceState;
  if (bytes == 0) {
    *dev_ptr = nullptr;
    return GpuStatus::kSuccess;
  }
  const cudaError_t err = cudaMalloc(dev_ptr, bytes);
  return map_cuda_error(err);
}

GpuStatus free_device_memory(void* dev_ptr) noexcept {
  if (!dev_ptr) return GpuStatus::kSuccess;
  const cudaError_t err = cudaFree(dev_ptr);
  return map_cuda_error(err);
}

GpuStatus copy_host_to_device(void* dev_dst, const void* host_src,
                              std::size_t bytes) noexcept {
  if (bytes == 0) return GpuStatus::kSuccess;
  if (!dev_dst || !host_src) return GpuStatus::kInvalidDeviceState;
  const cudaError_t err = cudaMemcpy(dev_dst, host_src, bytes, cudaMemcpyHostToDevice);
  return map_cuda_error(err);
}

GpuStatus copy_device_to_host(void* host_dst, const void* dev_src,
                              std::size_t bytes) noexcept {
  if (bytes == 0) return GpuStatus::kSuccess;
  if (!host_dst || !dev_src) return GpuStatus::kInvalidDeviceState;
  const cudaError_t err = cudaMemcpy(host_dst, dev_src, bytes, cudaMemcpyDeviceToHost);
  return map_cuda_error(err);
}

GpuStatus memset_device(void* dev_ptr, int value, std::size_t bytes) noexcept {
  if (bytes == 0) return GpuStatus::kSuccess;
  if (!dev_ptr) return GpuStatus::kInvalidDeviceState;
  const cudaError_t err = cudaMemset(dev_ptr, value, bytes);
  return map_cuda_error(err);
}

}  // namespace sankhya::gpu

#endif  // SANKHYA_ENABLE_CUDA
