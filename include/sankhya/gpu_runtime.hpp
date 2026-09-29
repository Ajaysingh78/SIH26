// SPDX-License-Identifier: Apache-2.0
// SANKHYA - CUDA runtime foundation layer (Phase 2 Feature 1).
//
// Minimum reusable CUDA runtime interface:
//  - Device discovery and property inspection
//  - Error handling with explicit status codes
//  - Safe RAII device memory allocation and transfers
//  - Device synchronization and non-finite output validation
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace sankhya::gpu {

/// Explicit status codes for all GPU runtime operations.
enum class GpuStatus {
  kSuccess = 0,
  kCudaUnavailable = 1,       ///< Build lacks CUDA or driver library not found
  kNoCompatibleDevice = 2,    ///< No CUDA-capable device installed in host system
  kInitFailed = 3,            ///< Device initialization or context creation failure
  kOutOfMemory = 4,           ///< cudaMalloc failed due to insufficient device memory
  kKernelLaunchFailed = 5,    ///< Kernel launch parameters invalid or launch failed
  kCopyFailed = 6,            ///< Host <-> Device memory transfer error
  kSyncFailed = 7,            ///< Device synchronization returned an error
  kInvalidDeviceState = 8,    ///< Invalid pointer, unallocated buffer, or wrong device
  kNonFiniteOutput = 9        ///< Computation produced NaN or Inf (numerical safety guard)
};

[[nodiscard]] constexpr std::string_view to_string(GpuStatus status) noexcept {
  switch (status) {
    case GpuStatus::kSuccess:
      return "success";
    case GpuStatus::kCudaUnavailable:
      return "cuda unavailable";
    case GpuStatus::kNoCompatibleDevice:
      return "no compatible cuda device";
    case GpuStatus::kInitFailed:
      return "initialization failed";
    case GpuStatus::kOutOfMemory:
      return "out of device memory";
    case GpuStatus::kKernelLaunchFailed:
      return "kernel launch failed";
    case GpuStatus::kCopyFailed:
      return "memory copy failed";
    case GpuStatus::kSyncFailed:
      return "device synchronization failed";
    case GpuStatus::kInvalidDeviceState:
      return "invalid device state";
    case GpuStatus::kNonFiniteOutput:
      return "non-finite numerical output detected";
  }
  return "unknown status";
}

/// Description of a CUDA-capable physical device.
struct GpuDeviceInfo {
  int device_id = -1;
  std::string name;
  std::size_t total_memory_bytes = 0;
  int compute_capability_major = 0;
  int compute_capability_minor = 0;
  int multiprocessor_count = 0;
  int warp_size = 32;
  int max_threads_per_block = 1024;
};

/// Returns true if CUDA is compiled into the binary AND at least one device is available.
[[nodiscard]] bool is_cuda_available() noexcept;

/// Returns number of available CUDA devices. Returns 0 if CUDA is not available.
[[nodiscard]] int get_gpu_device_count() noexcept;

/// Query properties for a given device index.
[[nodiscard]] GpuStatus get_device_info(int device_id, GpuDeviceInfo* out_info) noexcept;

/// Select active device.
[[nodiscard]] GpuStatus set_device(int device_id) noexcept;

/// Synchronize the active device.
[[nodiscard]] GpuStatus synchronize_device() noexcept;

/// Description of the last runtime error encountered.
[[nodiscard]] std::string get_last_error_string() noexcept;

// ---- Low-Level Memory Management --------------------------------------------------------

/// Allocate raw bytes on current device. Sets *dev_ptr to nullptr on failure.
[[nodiscard]] GpuStatus allocate_device_memory(void** dev_ptr, std::size_t bytes) noexcept;

/// Free raw bytes previously allocated on device.
[[nodiscard]] GpuStatus free_device_memory(void* dev_ptr) noexcept;

/// Copy bytes from host to device memory.
[[nodiscard]] GpuStatus copy_host_to_device(void* dev_dst, const void* host_src,
                                            std::size_t bytes) noexcept;

/// Copy bytes from device to host memory.
[[nodiscard]] GpuStatus copy_device_to_host(void* host_dst, const void* dev_src,
                                            std::size_t bytes) noexcept;

/// Set device memory bytes to a specific value.
[[nodiscard]] GpuStatus memset_device(void* dev_ptr, int value, std::size_t bytes) noexcept;

// ---- RAII Device Buffer -----------------------------------------------------------------

/// Minimal RAII GPU device buffer. Safe against leaks, non-copyable, movable.
template <typename T>
class DeviceBuffer {
 public:
  DeviceBuffer() = default;

  explicit DeviceBuffer(std::size_t count) {
    allocate(count);
  }

  ~DeviceBuffer() {
    free();
  }

  // Non-copyable to prevent accidental double-frees
  DeviceBuffer(const DeviceBuffer&) = delete;
  DeviceBuffer& operator=(const DeviceBuffer&) = delete;

  // Movable
  DeviceBuffer(DeviceBuffer&& other) noexcept
      : data_(other.data_), count_(other.count_) {
    other.data_ = nullptr;
    other.count_ = 0;
  }

  DeviceBuffer& operator=(DeviceBuffer&& other) noexcept {
    if (this != &other) {
      free();
      data_ = other.data_;
      count_ = other.count_;
      other.data_ = nullptr;
      other.count_ = 0;
    }
    return *this;
  }

  /// Allocate device memory for `count` elements of type T.
  GpuStatus allocate(std::size_t count) noexcept {
    free();
    if (count == 0) return GpuStatus::kSuccess;
    void* ptr = nullptr;
    const GpuStatus status = allocate_device_memory(&ptr, count * sizeof(T));
    if (status == GpuStatus::kSuccess) {
      data_ = static_cast<T*>(ptr);
      count_ = count;
    }
    return status;
  }

  /// Free allocated device memory.
  void free() noexcept {
    if (data_) {
      free_device_memory(data_);
      data_ = nullptr;
      count_ = 0;
    }
  }

  /// Upload data from host array.
  GpuStatus upload(const T* host_data, std::size_t count) noexcept {
    if (count == 0) return GpuStatus::kSuccess;
    if (count > count_) {
      const GpuStatus alloc_stat = allocate(count);
      if (alloc_stat != GpuStatus::kSuccess) return alloc_stat;
    }
    return copy_host_to_device(data_, host_data, count * sizeof(T));
  }

  /// Download data to host array.
  GpuStatus download(T* host_data, std::size_t count) const noexcept {
    if (count == 0) return GpuStatus::kSuccess;
    if (!data_ || count > count_) return GpuStatus::kInvalidDeviceState;
    return copy_device_to_host(host_data, data_, count * sizeof(T));
  }

  [[nodiscard]] T* data() noexcept { return data_; }
  [[nodiscard]] const T* data() const noexcept { return data_; }
  [[nodiscard]] std::size_t size() const noexcept { return count_; }
  [[nodiscard]] bool empty() const noexcept { return count_ == 0; }

 private:
  T* data_ = nullptr;
  std::size_t count_ = 0;
};

}  // namespace sankhya::gpu
