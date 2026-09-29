// SPDX-License-Identifier: Apache-2.0
// SANKHYA - Adaptive CPU/GPU/Hybrid Solver Selection (Phase 2 Feature 2).
#include "sankhya/adaptive.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include <fmt/format.h>

#include "sankhya/gpu_runtime.hpp"

namespace sankhya {

std::string BackendDecision::format_explanation() const {
  std::ostringstream ss;
  ss << "Backend decision: " << to_string(selected_backend);
  if (!is_valid) {
    ss << " [REJECTED: requested backend unavailable]";
  }
  ss << "\nPrimary reason: " << primary_reason;
  if (!detailed_reasons.empty()) {
    ss << "\nDetailed factors:";
    for (const auto& r : detailed_reasons) {
      ss << "\n  - " << r;
    }
  }
  return ss.str();
}

ProblemCharacteristics analyze_problem(const Model& model) {
  ProblemCharacteristics chars;
  chars.num_rows = model.num_rows();
  chars.num_cols = model.num_cols();
  chars.num_nonzeros = model.num_nonzeros();
  chars.num_integer_cols = model.num_integer_columns();
  chars.has_quadratic_objective = model.has_quadratic_objective();

  if (chars.num_integer_cols > 0 && chars.has_quadratic_objective) {
    chars.problem_class_name = "MIQP";
  } else if (chars.num_integer_cols > 0) {
    chars.problem_class_name = "MILP";
  } else if (chars.has_quadratic_objective) {
    chars.problem_class_name = "QP";
  } else {
    chars.problem_class_name = "LP";
  }

  const double total_entries = static_cast<double>(chars.num_rows) * static_cast<double>(chars.num_cols);
  if (total_entries > 0.0) {
    chars.density = static_cast<double>(chars.num_nonzeros) / total_entries;
    chars.sparsity_ratio = 1.0 - chars.density;
  } else {
    chars.density = 0.0;
    chars.sparsity_ratio = 1.0;
  }

  // Estimated operations: ~2 flops per nonzero for a single matrix-vector product
  chars.estimated_matvec_flops = 2.0 * static_cast<double>(chars.num_nonzeros);

  // Estimated memory footprint: CSC format (col_starts + row_indices + values) + dense vectors
  const double csc_bytes = static_cast<double>(chars.num_nonzeros) * (sizeof(Index) + sizeof(double)) +
                           static_cast<double>(chars.num_cols + 1) * sizeof(Index);
  const double vec_bytes = static_cast<double>(chars.num_rows + chars.num_cols) * sizeof(double) * 4.0;
  chars.estimated_memory_bytes = csc_bytes + vec_bytes;

  // Query hardware environment state
  chars.gpu_available = gpu::is_cuda_available();
  chars.gpu_device_count = gpu::get_gpu_device_count();
  if (chars.gpu_available && chars.gpu_device_count > 0) {
    gpu::GpuDeviceInfo dev_info;
    if (gpu::get_device_info(0, &dev_info) == gpu::GpuStatus::kSuccess) {
      chars.gpu_name = dev_info.name;
      chars.gpu_memory_bytes = dev_info.total_memory_bytes;
    }
  }

  return chars;
}

AdaptiveConfig load_adaptive_config(const Options& options) {
  AdaptiveConfig cfg;
  if (options.has_option("adaptive_min_nnz")) {
    cfg.min_gpu_nonzeros = static_cast<Index>(options.get_int("adaptive_min_nnz"));
  }
  if (options.has_option("adaptive_min_dim")) {
    cfg.min_gpu_dimension = static_cast<Index>(options.get_int("adaptive_min_dim"));
  }
  if (options.has_option("adaptive_min_density")) {
    cfg.min_gpu_density = options.get_double("adaptive_min_density");
  }
  if (options.has_option("adaptive_explain")) {
    cfg.explain_decision = options.get_bool("adaptive_explain");
  }
  return cfg;
}

BackendDecision select_backend(const ProblemCharacteristics& chars,
                               const Options& options,
                               const AdaptiveConfig& config) {
  BackendDecision decision;

  // Determine requested mode
  BackendMode mode = BackendMode::kAuto;
  bool explicit_backend_given = false;
  if (options.has_option("backend")) {
    const std::string b_str = options.get_string("backend");
    if (b_str != "auto") {
      explicit_backend_given = parse_backend_mode(b_str, &mode);
    }
  }

  // Backwards compatibility for legacy --gpu bool flag
  const bool legacy_gpu_flag =
      (!explicit_backend_given && options.has_option("gpu") && options.get_bool("gpu"));

  if (legacy_gpu_flag) {
    if (chars.gpu_available) {
      mode = BackendMode::kGpu;
    } else {
      // Legacy behavior: warn and fall back to CPU without failing
      decision.selected_backend = DeviceBackend::kCpu;
      decision.requested_mode = BackendMode::kAuto;
      decision.is_valid = true;
      decision.primary_reason =
          "--gpu requested but CUDA is not available in this environment/build; running on CPU";
      decision.detailed_reasons.push_back("Legacy --gpu option permits graceful fallback to CPU");
      return decision;
    }
  }

  decision.requested_mode = mode;

  // 1. Explicit CPU request
  if (mode == BackendMode::kCpu) {
    decision.selected_backend = DeviceBackend::kCpu;
    decision.is_valid = true;
    decision.primary_reason = "Explicitly requested CPU backend (--backend=cpu)";
    decision.detailed_reasons.push_back("User override forces CPU execution");
    return decision;
  }

  // 2. Explicit GPU request (--backend=gpu)
  if (mode == BackendMode::kGpu) {
    decision.selected_backend = DeviceBackend::kGpu;
    if (chars.gpu_available) {
      decision.is_valid = true;
      decision.primary_reason = "Explicitly requested GPU backend (--backend=gpu); CUDA device ready";
      decision.detailed_reasons.push_back(fmt::format("Device 0: {}", chars.gpu_name.empty() ? "CUDA Device" : chars.gpu_name));
    } else {
      // Must NOT silently fall back: explicit GPU request fails cleanly if unavailable
      decision.is_valid = false;
      decision.primary_reason = "Explicitly requested GPU backend (--backend=gpu), but CUDA device is not available";
      decision.detailed_reasons.push_back("No CUDA-capable GPU detected or CUDA runtime is unavailable on this system");
      decision.detailed_reasons.push_back("Explicit GPU mode refuses silent fallback to ensure predictable optimization");
    }
    return decision;
  }

  // 3. Explicit Hybrid request
  if (mode == BackendMode::kHybrid) {
    decision.selected_backend = DeviceBackend::kHybrid;
    decision.is_valid = false;
    decision.primary_reason = "Explicitly requested Hybrid backend (--backend=hybrid), which is unsupported in this version";
    decision.detailed_reasons.push_back("Coordinated CPU/GPU hybrid execution path is reserved for future implementation");
    return decision;
  }

  // 4. AUTO mode: Adaptive problem-driven selection
  decision.requested_mode = BackendMode::kAuto;

  // Condition 1: Check hardware availability
  if (!chars.gpu_available) {
    decision.selected_backend = DeviceBackend::kCpu;
    decision.is_valid = true;
    decision.primary_reason = "Adaptive selector chose CPU: CUDA GPU is not available in this environment";
    decision.detailed_reasons.push_back("Host system does not report an available CUDA-capable device");
    decision.detailed_reasons.push_back("Safe fallback to deterministic CPU solver");
    return decision;
  }

  // Condition 2: Problem Class Compatibility
  if (chars.num_integer_cols > 0) {
    decision.selected_backend = DeviceBackend::kCpu;
    decision.is_valid = true;
    decision.primary_reason = fmt::format(
        "Adaptive selector chose CPU: problem is {}, which is optimized for CPU branch-and-cut",
        chars.problem_class_name);
    decision.detailed_reasons.push_back("MIP branch-and-cut tree exploration runs on host CPU");
    return decision;
  }

  // Condition 3: Workload Dimensions and Nonzeros (PCIe Transfer vs Kernel Amortization)
  const Index max_dim = std::max(chars.num_rows, chars.num_cols);
  const bool has_sufficient_nonzeros = (chars.num_nonzeros >= config.min_gpu_nonzeros);
  const bool has_sufficient_dimension = (max_dim >= config.min_gpu_dimension);

  if (!has_sufficient_nonzeros || !has_sufficient_dimension) {
    decision.selected_backend = DeviceBackend::kCpu;
    decision.is_valid = true;
    decision.primary_reason = fmt::format(
        "Adaptive selector chose CPU: problem size ({} nonzeros, {}x{}) is below GPU threshold ({} nonzeros, {} min dimension)",
        chars.num_nonzeros, chars.num_rows, chars.num_cols, config.min_gpu_nonzeros, config.min_gpu_dimension);
    decision.detailed_reasons.push_back("Small problem workload does not justify host-to-device PCIe transfer overhead");
    decision.detailed_reasons.push_back("CPU cache and SIMD registers offer superior throughput on small workloads");
    return decision;
  }

  // Condition 4: GPU Memory Headroom
  if (chars.gpu_memory_bytes > 0 &&
      chars.estimated_memory_bytes > static_cast<double>(chars.gpu_memory_bytes) * config.max_gpu_memory_headroom) {
    decision.selected_backend = DeviceBackend::kCpu;
    decision.is_valid = true;
    decision.primary_reason = "Adaptive selector chose CPU: estimated model memory exceeds safe GPU memory headroom";
    decision.detailed_reasons.push_back(fmt::format(
        "Model requires ~{:.1f} MB, exceeding {:.0f}% of device capacity ({:.1f} MB)",
        chars.estimated_memory_bytes / (1024.0 * 1024.0),
        config.max_gpu_memory_headroom * 100.0,
        static_cast<double>(chars.gpu_memory_bytes) / (1024.0 * 1024.0)));
    return decision;
  }

  // All criteria satisfied: offload to GPU
  decision.selected_backend = DeviceBackend::kGpu;
  decision.is_valid = true;
  decision.primary_reason = fmt::format(
      "Adaptive selector chose GPU: problem size ({} nonzeros, {}x{}) exceeds GPU acceleration threshold",
      chars.num_nonzeros, chars.num_rows, chars.num_cols);
  decision.detailed_reasons.push_back(fmt::format("CUDA device '{}' has sufficient VRAM", chars.gpu_name.empty() ? "default" : chars.gpu_name));
  decision.detailed_reasons.push_back("Sparse matrix-vector operations parallelize across GPU streaming multiprocessors");
  return decision;
}

}  // namespace sankhya
