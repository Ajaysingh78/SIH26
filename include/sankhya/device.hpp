// SPDX-License-Identifier: Apache-2.0
// SANKHYA - execution device abstraction (Phase 2 Feature 1).
//
// Distinguishes execution targets: CPU, GPU, and future Hybrid execution.
// This interface defines the contract between solver algorithms and physical
// execution hardware without hardcoding hardware-specific dependencies.
#pragma once

#include <string>
#include <string_view>

#include "sankhya/options.hpp"

namespace sankhya {

/// Hardware execution target.
enum class DeviceBackend {
  kCpu = 0,     ///< Standard host CPU execution (deterministic, exact basis support)
  kGpu = 1,     ///< Accelerated device GPU execution (CUDA, massive parallelism)
  kHybrid = 2   ///< Coordinated CPU+GPU execution (Reserved for Phase 2 Feature 2)
};

/// Convert DeviceBackend to a human-readable string.
[[nodiscard]] constexpr std::string_view to_string(DeviceBackend backend) noexcept {
  switch (backend) {
    case DeviceBackend::kCpu:
      return "cpu";
    case DeviceBackend::kGpu:
      return "gpu";
    case DeviceBackend::kHybrid:
      return "hybrid";
  }
  return "unknown";
}

/// Parse string into DeviceBackend. Returns false on unknown token.
[[nodiscard]] inline bool parse_device_backend(std::string_view str, DeviceBackend* out) noexcept {
  if (!out) return false;
  if (str == "cpu" || str == "CPU") {
    *out = DeviceBackend::kCpu;
    return true;
  }
  if (str == "gpu" || str == "cuda" || str == "GPU" || str == "CUDA") {
    *out = DeviceBackend::kGpu;
    return true;
  }
  if (str == "hybrid" || str == "HYBRID") {
    *out = DeviceBackend::kHybrid;
    return true;
  }
  return false;
}

/// Execution context specifying target device and GPU configuration.
struct ExecutionContext {
  DeviceBackend backend = DeviceBackend::kCpu;
  int device_id = 0;

  [[nodiscard]] bool is_gpu() const noexcept { return backend == DeviceBackend::kGpu; }
  [[nodiscard]] bool is_cpu() const noexcept { return backend == DeviceBackend::kCpu; }
  [[nodiscard]] bool is_hybrid() const noexcept { return backend == DeviceBackend::kHybrid; }
};

/// Determine requested execution backend from solver options.
/// Note: Explicit CPU or GPU selection only. Adaptive routing belongs to Feature 2.
[[nodiscard]] inline ExecutionContext resolve_execution_context(const Options& options) noexcept {
  ExecutionContext ctx;
  if (options.get_bool("gpu")) {
    ctx.backend = DeviceBackend::kGpu;
  } else {
    ctx.backend = DeviceBackend::kCpu;
  }
  return ctx;
}

}  // namespace sankhya
