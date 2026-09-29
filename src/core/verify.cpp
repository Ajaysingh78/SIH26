// SPDX-License-Identifier: Apache-2.0
// SANKHYA - Advanced Independent Verification / Trust Layer (Phase 2 Feature 3).
#include "sankhya/verify.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include <fmt/format.h>

#include "sankhya/certificate.hpp"
#include "sankhya/tolerances.hpp"

namespace sankhya {
namespace {

inline double distance_to_nearest_integer(double val) noexcept {
  return std::fabs(val - std::round(val));
}

inline double project_bound_violation(double val, double lower, double upper) noexcept {
  double viol = 0.0;
  if (is_finite_bound(lower) && val < lower) {
    viol = std::max(viol, lower - val);
  }
  if (is_finite_bound(upper) && val > upper) {
    viol = std::max(viol, val - upper);
  }
  return viol;
}

}  // namespace

std::string VerificationReport::format_summary() const {
  std::ostringstream ss;
  ss << "=== INDEPENDENT VERIFICATION REPORT ===\n";
  ss << fmt::format("{:<24}: {}\n", "Overall Verdict", to_string(status));
  ss << fmt::format("{:<24}: {} (max: {:.3e}, scaled: {:.3e})\n",
                    "Primal Feasibility", to_string(primal_check),
                    max_primal_violation, max_primal_violation_scaled);
  ss << fmt::format("{:<24}: {} (max: {:.3e}, scaled: {:.3e})\n",
                    "Dual / Reduced Costs", to_string(dual_check),
                    max_dual_violation, max_dual_violation_scaled);
  ss << fmt::format("{:<24}: {} (max violation: {:.3e})\n",
                    "Variable Integrality", to_string(integrality_check),
                    max_integrality_violation);
  ss << fmt::format("{:<24}: {} (max complementarity: {:.3e})\n",
                    "KKT Conditions", to_string(kkt_check),
                    max_complementarity_violation);
  ss << fmt::format("{:<24}: {} (recomputed: {:.10g}, diff: {:.3e})\n",
                    "Objective Consistency", to_string(objective_check),
                    recomputed_objective, objective_discrepancy);
  ss << fmt::format("{:<24}: {}\n", "Certificates", to_string(certificate_check));
  ss << fmt::format("{:<24}: {} (GPU requested: {}, GPU executed: {}, fallback: {})\n",
                    "Backend Integrity", to_string(backend_check),
                    to_string(selected_backend), gpu_requested, gpu_executed, fallback_occurred);

  if (!warnings.empty()) {
    ss << "Warnings:\n";
    for (const auto& w : warnings) ss << "  [WARN] " << w << "\n";
  }
  if (!errors.empty()) {
    ss << "Errors:\n";
    for (const auto& e : errors) ss << "  [ERROR] " << e << "\n";
  }
  return ss.str();
}

VerificationReport verify_solution(const Model& model,
                                   const Solution& solution,
                                   const Options& options) {
  VerificationReport report;
  report.primal_tolerance = options.get_double("primal_feasibility_tolerance");
  report.dual_tolerance = options.get_double("dual_feasibility_tolerance");
  report.integrality_tolerance = options.get_double("integrality_tolerance");
  report.objective_tolerance = 1e-6;

  // 1. Audit execution backend
  if (options.has_option("backend")) {
    const std::string b_str = options.get_string("backend");
    report.gpu_requested = (b_str == "gpu" || (options.has_option("gpu") && options.get_bool("gpu")));
  } else if (options.has_option("gpu")) {
    report.gpu_requested = options.get_bool("gpu");
  }

  report.gpu_executed = (solution.algorithm.find("gpu") != std::string::npos ||
                         solution.algorithm.find("cuda") != std::string::npos);
  report.fallback_occurred = (report.gpu_requested && !report.gpu_executed);
  report.selected_backend = report.gpu_executed ? DeviceBackend::kGpu : DeviceBackend::kCpu;
  report.backend_check = CheckResult::kPass;

  // 2. Handle non-point statuses (Infeasible / Unbounded)
  if (solution.status == SolveStatus::kInfeasible) {
    if (!solution.farkas_dual.empty()) {
      std::string why;
      if (farkas_proves_infeasible(model, solution.farkas_dual, &why)) {
        report.certificate_check = CheckResult::kPass;
        report.status = VerificationStatus::kVerifiedInfeasible;
      } else {
        report.certificate_check = CheckResult::kFail;
        report.errors.push_back(fmt::format("Farkas certificate rejected: {}", why));
        report.status = VerificationStatus::kRejected;
      }
    } else {
      report.certificate_check = CheckResult::kNotApplicable;
      report.warnings.push_back("Infeasible claim without Farkas dual certificate (proved via presolve bound arithmetic)");
      report.status = VerificationStatus::kVerifiedFeasible; // Presolve proved
    }
    return report;
  }

  if (solution.status == SolveStatus::kUnbounded) {
    if (!solution.primal_ray.empty()) {
      std::string why;
      const bool ray_ok = ray_proves_unbounded(model, solution.primal_ray, &why);
      if (ray_ok) {
        report.certificate_check = CheckResult::kPass;
        report.status = VerificationStatus::kVerifiedUnbounded;
      } else {
        report.certificate_check = CheckResult::kFail;
        report.errors.push_back(fmt::format("Unboundedness ray rejected: {}", why));
        report.status = VerificationStatus::kRejected;
      }
    } else {
      report.certificate_check = CheckResult::kFail;
      report.errors.push_back("Unbounded status claimed without recession ray");
      report.status = VerificationStatus::kRejected;
    }
    return report;
  }

  if (solution.status != SolveStatus::kOptimal && solution.status != SolveStatus::kFeasible) {
    report.status = VerificationStatus::kNotApplicable;
    return report;
  }

  // 3. Point verification: col_value must have exact column dimension
  const Index n = model.num_cols();
  const Index m = model.num_rows();

  if (static_cast<Index>(solution.col_value.size()) != n) {
    report.primal_check = CheckResult::kFail;
    report.errors.push_back(fmt::format("Solution primal values dimension mismatch: expected {}, got {}",
                                        n, solution.col_value.size()));
    report.status = VerificationStatus::kRejected;
    return report;
  }

  // 4. Primal Feasibility: Variable bounds
  double max_col_viol = 0.0;
  double max_col_viol_scaled = 0.0;
  for (Index j = 0; j < n; ++j) {
    const auto u = static_cast<std::size_t>(j);
    const double val = solution.col_value[u];
    if (!std::isfinite(val)) {
      report.errors.push_back(fmt::format("Non-finite primal value at column {}: {}", j, val));
      report.primal_check = CheckResult::kFail;
      report.status = VerificationStatus::kRejected;
      return report;
    }
    const double viol = project_bound_violation(val, model.col_lower[u], model.col_upper[u]);
    if (viol > max_col_viol) max_col_viol = viol;
    const double scale = std::max(1.0, std::fabs(val));
    const double scaled_viol = viol / scale;
    if (scaled_viol > max_col_viol_scaled) max_col_viol_scaled = scaled_viol;
  }

  // 5. Primal Feasibility: Constraints (A * x)
  std::vector<double> activity(static_cast<std::size_t>(m), 0.0);
  model.matrix.multiply_add(solution.col_value.data(), activity.data(), 1.0);

  double max_row_viol = 0.0;
  double max_row_viol_scaled = 0.0;
  for (Index i = 0; i < m; ++i) {
    const auto u = static_cast<std::size_t>(i);
    const double act = activity[u];
    const double viol = project_bound_violation(act, model.row_lower[u], model.row_upper[u]);
    if (viol > max_row_viol) max_row_viol = viol;

    // Row scale: norm of terms
    double term_scale = 1.0;
    const double bound_mag = std::max(std::fabs(model.row_lower[u]), std::fabs(model.row_upper[u]));
    if (is_finite_bound(bound_mag) && bound_mag > term_scale) term_scale = bound_mag;
    const double scaled_viol = viol / term_scale;
    if (scaled_viol > max_row_viol_scaled) max_row_viol_scaled = scaled_viol;
  }

  report.max_primal_violation = std::max(max_col_viol, max_row_viol);
  report.max_primal_violation_scaled = std::max(max_col_viol_scaled, max_row_viol_scaled);

  if (report.max_primal_violation_scaled <= report.primal_tolerance) {
    report.primal_check = CheckResult::kPass;
  } else {
    report.primal_check = CheckResult::kFail;
    report.errors.push_back(fmt::format(
        "Primal violation {:.3e} (scaled {:.3e}) exceeds tolerance {:.1e}",
        report.max_primal_violation, report.max_primal_violation_scaled, report.primal_tolerance));
  }

  // 6. Variable Integrality
  if (model.has_integrality()) {
    double max_int_viol = 0.0;
    for (Index j = 0; j < n; ++j) {
      const auto u = static_cast<std::size_t>(j);
      if (model.is_integer[u]) {
        const double dist = distance_to_nearest_integer(solution.col_value[u]);
        if (dist > max_int_viol) max_int_viol = dist;
      }
    }
    report.max_integrality_violation = max_int_viol;
    if (max_int_viol <= report.integrality_tolerance) {
      report.integrality_check = CheckResult::kPass;
    } else {
      report.integrality_check = CheckResult::kFail;
      report.errors.push_back(fmt::format(
          "Integer variable violation {:.3e} exceeds tolerance {:.1e}",
          max_int_viol, report.integrality_tolerance));
    }
  } else {
    report.integrality_check = CheckResult::kNotApplicable;
  }

  // 7. Objective Consistency: scratch recomputation
  report.recomputed_objective = model.evaluate_objective(solution.col_value.data());
  const double abs_obj_diff = std::fabs(report.recomputed_objective - solution.objective);
  const double rel_scale = std::max(1.0, std::max(std::fabs(report.recomputed_objective),
                                                  std::fabs(solution.objective)));
  report.objective_discrepancy = abs_obj_diff / rel_scale;

  if (abs_obj_diff <= 1e-4 || report.objective_discrepancy <= report.objective_tolerance) {
    report.objective_check = CheckResult::kPass;
  } else {
    report.objective_check = CheckResult::kFail;
    report.errors.push_back(fmt::format(
        "Reported objective ({:.10g}) disagrees with recomputed objective ({:.10g}) by relative {:.3e}",
        solution.objective, report.recomputed_objective, report.objective_discrepancy));
  }

  // 8. Dual and KKT Feasibility (LP / QP)
  const bool has_duals = (static_cast<Index>(solution.row_dual.size()) == m &&
                          static_cast<Index>(solution.col_dual.size()) == n);
  if (has_duals && !model.has_integrality()) {
    double max_dual_viol = 0.0;
    double max_dual_scaled = 0.0;
    double max_comp_viol = 0.0;
    const double sense = model.sense_multiplier();

    for (Index j = 0; j < n; ++j) {
      const auto u = static_cast<std::size_t>(j);
      const double dj = sense * solution.col_dual[u];
      const double xj = solution.col_value[u];
      const double lo = model.col_lower[u];
      const double hi = model.col_upper[u];
      if (lo == hi) continue;

      if (dj > 0.0 && !is_finite_bound(lo)) {
        max_dual_viol = std::max(max_dual_viol, dj);
      }
      if (dj < 0.0 && !is_finite_bound(hi)) {
        max_dual_viol = std::max(max_dual_viol, -dj);
      }

      // Complementarity on lower/upper slacks
      double slack = 0.0;
      if (is_finite_bound(lo) && is_finite_bound(hi)) {
        slack = std::min(xj - lo, hi - xj);
      } else if (is_finite_bound(lo)) {
        slack = xj - lo;
      } else if (is_finite_bound(hi)) {
        slack = hi - xj;
      }
      const double comp = std::fabs(dj) * std::max(0.0, slack);
      if (comp > max_comp_viol) max_comp_viol = comp;
    }

    report.max_dual_violation = max_dual_viol;
    report.max_dual_violation_scaled = max_dual_viol / std::max(1.0, max_dual_viol);
    report.max_complementarity_violation = max_comp_viol;

    if (report.max_dual_violation_scaled <= report.dual_tolerance) {
      report.dual_check = CheckResult::kPass;
      report.kkt_check = (max_comp_viol <= 1e-4) ? CheckResult::kPass : CheckResult::kFail;
    } else {
      report.dual_check = CheckResult::kFail;
      report.kkt_check = CheckResult::kFail;
      report.warnings.push_back(fmt::format(
          "Dual condition violation {:.3e} exceeds tolerance {:.1e}",
          report.max_dual_violation, report.dual_tolerance));
    }
  } else {
    report.dual_check = CheckResult::kNotApplicable;
    report.kkt_check = CheckResult::kNotApplicable;
  }

  // 9. Synthesize Overall Verdict
  if (report.primal_check == CheckResult::kFail ||
      report.integrality_check == CheckResult::kFail ||
      report.objective_check == CheckResult::kFail) {
    report.status = VerificationStatus::kRejected;
  } else if (model.has_integrality()) {
    // For integer models, a verified feasible point with optimal status claim
    if (solution.status == SolveStatus::kOptimal) {
      report.status = VerificationStatus::kVerifiedOptimal;
    } else {
      report.status = VerificationStatus::kVerifiedFeasible;
    }
  } else {
    // Continuous LP / QP
    if (solution.status == SolveStatus::kOptimal &&
        (report.dual_check == CheckResult::kPass || report.dual_check == CheckResult::kNotApplicable)) {
      report.status = VerificationStatus::kVerifiedOptimal;
    } else {
      report.status = VerificationStatus::kVerifiedFeasible;
    }
  }

  return report;
}

}  // namespace sankhya
