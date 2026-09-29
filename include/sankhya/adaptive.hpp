// SPDX-License-Identifier: Apache-2.0
// SANKHYA - Adaptive CPU/GPU/Hybrid Solver Selection (Phase 2 Feature 2).
//
// Analyzes optimization problems before solving and selects the most appropriate
// execution backend (CPU, GPU, or HYBRID) based on measurable model characteristics
// and available hardware.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "sankhya/device.hpp"
#include "sankhya/model.hpp"
#include "sankhya/options.hpp"
#include "sankhya/types.hpp"

namespace sankhya {

/// User-requested execution mode.
enum class BackendMode {
  kAuto = 0,    ///< Adaptively analyze problem and select best available backend
  kCpu = 1,     ///< Explicitly force CPU execution
  kGpu = 2,     ///< Explicitly force GPU execution (fails if GPU is unavailable)
  kHybrid = 3   ///< Explicitly request Hybrid execution (reserved / unsupported)
};

[[nodiscard]] constexpr std::string_view to_string(BackendMode mode) noexcept {
  switch (mode) {
    case BackendMode::kAuto:
      return "auto";
    case BackendMode::kCpu:
      return "cpu";
    case BackendMode::kGpu:
      return "gpu";
    case BackendMode::kHybrid:
      return "hybrid";
  }
  return "unknown";
}

[[nodiscard]] inline bool parse_backend_mode(std::string_view str, BackendMode* out) noexcept {
  if (!out) return false;
  if (str == "auto" || str == "AUTO") {
    *out = BackendMode::kAuto;
    return true;
  }
  if (str == "cpu" || str == "CPU") {
    *out = BackendMode::kCpu;
    return true;
  }
  if (str == "gpu" || str == "GPU" || str == "cuda" || str == "CUDA") {
    *out = BackendMode::kGpu;
    return true;
  }
  if (str == "hybrid" || str == "HYBRID") {
    *out = BackendMode::kHybrid;
    return true;
  }
  return false;
}

/// Measured characteristics of an optimization model.
struct ProblemCharacteristics {
  std::string problem_class_name;      ///< "LP", "MILP", "QP", "MIQP"
  Index num_rows = 0;
  Index num_cols = 0;
  Index num_nonzeros = 0;
  double density = 0.0;                 ///< nnz / (rows * cols)
  double sparsity_ratio = 1.0;          ///< 1.0 - density
  Index num_integer_cols = 0;
  bool has_quadratic_objective = false;
  double estimated_matvec_flops = 0.0;  ///< ~ 2 * nnz per SpMV
  double estimated_memory_bytes = 0.0;  ///< Matrix + vector storage

  // Hardware environment characteristics
  bool gpu_available = false;
  int gpu_device_count = 0;
  std::string gpu_name;
  std::size_t gpu_memory_bytes = 0;
};

/// Configurable thresholds for the adaptive selection policy.
/// These initial heuristic values are conservative and derived from memory
/// transfer latency vs GPU kernel bandwidth trade-offs.
struct AdaptiveConfig {
  Index min_gpu_nonzeros = 50000;       ///< Minimum nonzeros to amortize host-device transfer
  Index min_gpu_dimension = 1000;       ///< Minimum dimension (rows or columns)
  double min_gpu_density = 1e-6;        ///< Minimum density to avoid extreme hyper-sparsity
  double max_gpu_memory_headroom = 0.8; ///< Maximum fraction of GPU VRAM permitted
  bool explain_decision = true;         ///< Whether to generate human-readable explanation
};

/// The outcome of the backend selection process.
struct BackendDecision {
  DeviceBackend selected_backend = DeviceBackend::kCpu;
  BackendMode requested_mode = BackendMode::kAuto;
  bool is_valid = true;                 ///< True if execution can proceed; false if requested backend cannot run
  std::string primary_reason;
  std::vector<std::string> detailed_reasons;

  [[nodiscard]] std::string format_explanation() const;
};

/// Extract measurable problem and hardware characteristics from a Model and environment.
[[nodiscard]] ProblemCharacteristics analyze_problem(const Model& model);

/// Load adaptive configuration thresholds from Options.
[[nodiscard]] AdaptiveConfig load_adaptive_config(const Options& options);

/// Evaluate problem characteristics and user options to decide the execution backend.
/// Strictly adheres to:
///  - Explicit CPU: selects CPU
///  - Explicit GPU: selects GPU if available; marks invalid (fails clearly) if unavailable
///  - Explicit Hybrid: marks invalid (unsupported)
///  - AUTO: selects GPU only when hardware is available and problem characteristics justify it;
///          otherwise safely falls back to CPU with transparent reasoning.
[[nodiscard]] BackendDecision select_backend(const ProblemCharacteristics& chars,
                                             const Options& options,
                                             const AdaptiveConfig& config);

}  // namespace sankhya
