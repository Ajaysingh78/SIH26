// SPDX-License-Identifier: Apache-2.0
// SANKHYA - Advanced Independent Verification / Trust Layer (Phase 2 Feature 3).
//
// Independently verifies solver solutions against the original Model:
//  - Primal feasibility (row and column bounds)
//  - Dual feasibility and KKT conditions (reduced costs, multipliers, complementarity)
//  - Objective consistency (scratch recomputation of c'x + 0.5 x'Qx)
//  - Variable integrality (MILP / MIQP)
//  - Certificates (Farkas for infeasibility, rays for unboundedness)
//  - Backend execution integrity (verifies hardware target and fallback reporting)
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

/// Overall trust verdict produced by independent verification.
enum class VerificationStatus {
  kVerifiedOptimal = 0,    ///< Feasibility, dual/KKT conditions, and objective independently verified
  kVerifiedFeasible = 1,   ///< Feasibility and integrality verified; optimality proof not available
  kVerifiedInfeasible = 2, ///< Infeasibility independently proved by Farkas certificate
  kVerifiedUnbounded = 3,  ///< Unboundedness independently proved by primal ray + feasible point
  kRejected = 4,           ///< Independent verification failed: constraint, bound, or objective violation
  kNotApplicable = 5       ///< Verification not applicable (model not solved, model error)
};

[[nodiscard]] constexpr std::string_view to_string(VerificationStatus status) noexcept {
  switch (status) {
    case VerificationStatus::kVerifiedOptimal:
      return "VERIFIED OPTIMAL";
    case VerificationStatus::kVerifiedFeasible:
      return "VERIFIED FEASIBLE";
    case VerificationStatus::kVerifiedInfeasible:
      return "VERIFIED INFEASIBLE";
    case VerificationStatus::kVerifiedUnbounded:
      return "VERIFIED UNBOUNDED";
    case VerificationStatus::kRejected:
      return "REJECTED";
    case VerificationStatus::kNotApplicable:
      return "NOT APPLICABLE";
  }
  return "UNKNOWN";
}

/// Status of individual component checks.
enum class CheckResult {
  kPass = 0,
  kFail = 1,
  kNotApplicable = 2
};

[[nodiscard]] constexpr std::string_view to_string(CheckResult result) noexcept {
  switch (result) {
    case CheckResult::kPass:
      return "PASS";
    case CheckResult::kFail:
      return "FAIL";
    case CheckResult::kNotApplicable:
      return "N/A";
  }
  return "UNKNOWN";
}

/// Structured report containing the outcome of all independent checks.
struct VerificationReport {
  VerificationStatus status = VerificationStatus::kNotApplicable;

  CheckResult primal_check = CheckResult::kNotApplicable;
  CheckResult dual_check = CheckResult::kNotApplicable;
  CheckResult integrality_check = CheckResult::kNotApplicable;
  CheckResult kkt_check = CheckResult::kNotApplicable;
  CheckResult objective_check = CheckResult::kNotApplicable;
  CheckResult certificate_check = CheckResult::kNotApplicable;
  CheckResult backend_check = CheckResult::kNotApplicable;

  // Numerical violations recomputed from scratch on the original model
  double max_primal_violation = 0.0;
  double max_primal_violation_scaled = 0.0;
  double max_dual_violation = 0.0;
  double max_dual_violation_scaled = 0.0;
  double max_integrality_violation = 0.0;
  double max_complementarity_violation = 0.0;
  double recomputed_objective = 0.0;
  double objective_discrepancy = 0.0;

  // Tolerances applied during verification
  double primal_tolerance = 1e-7;
  double dual_tolerance = 1e-7;
  double integrality_tolerance = 1e-6;
  double objective_tolerance = 1e-6;

  // Backend audit
  DeviceBackend selected_backend = DeviceBackend::kCpu;
  bool gpu_requested = false;
  bool gpu_executed = false;
  bool fallback_occurred = false;

  std::vector<std::string> errors;
  std::vector<std::string> warnings;

  [[nodiscard]] bool passed() const noexcept {
    return status == VerificationStatus::kVerifiedOptimal ||
           status == VerificationStatus::kVerifiedFeasible ||
           status == VerificationStatus::kVerifiedInfeasible ||
           status == VerificationStatus::kVerifiedUnbounded;
  }

  [[nodiscard]] std::string format_summary() const;
};

/// Perform an independent verification of the given Solution against the original Model.
/// Recomputes all quantities directly from Model data structures rather than trusting
/// solver internal status.
[[nodiscard]] VerificationReport verify_solution(const Model& model,
                                                 const Solution& solution,
                                                 const Options& options);

}  // namespace sankhya
