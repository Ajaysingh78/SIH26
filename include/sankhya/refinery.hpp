// SPDX-License-Identifier: Apache-2.0
// SANKHYA - MRPL-Style Refinery Optimization Digital Twin (Phase 2 Feature 4).
//
// =========================================================================================
// SYNTHETIC INDUSTRIAL BENCHMARK DISCLAIMER:
// This is an optimization-oriented synthetic refinery digital twin developed for
// demonstration, decision intelligence, and solver benchmarking. It uses public-domain
// chemical engineering parameters, synthetic crude assays, and idealized unit yields.
// IT DOES NOT CONTAIN OR REPRESENT CONFIDENTIAL OR PROPRIETARY MRPL OPERATIONAL DATA.
// =========================================================================================
#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "sankhya/adaptive.hpp"
#include "sankhya/model.hpp"
#include "sankhya/options.hpp"
#include "sankhya/verify.hpp"

namespace sankhya::refinery {

/// Crude feed specification for refinery distillation.
struct CrudeGrade {
  std::string name;              ///< Identifier (e.g., "Arab_Light", "Murban")
  double api_gravity = 34.0;     ///< API Gravity (degrees)
  double sulfur_wt_pct = 1.5;    ///< Sulfur content (% by weight)
  double price_per_bbl = 80.0;   ///< Delivered crude purchase price ($/bbl)
  double max_avail_kbpd = 60.0;  ///< Maximum availability (thousand barrels/day)

  // Atmospheric distillation volumetric cut yields (fractions summing <= 1.0)
  double yield_lpg = 0.03;       ///< Liquefied petroleum gas cut
  double yield_light_naphtha = 0.12; ///< Light naphtha (isomerization / blending)
  double yield_heavy_naphtha = 0.15; ///< Heavy naphtha (reformer feed)
  double yield_kerosene = 0.14;  ///< Kerosene / Jet A-1 cut
  double yield_lgo = 0.26;       ///< Light gas oil (diesel boiling range)
  double yield_heavy_residue = 0.30; ///< Atmospheric residue (VDU feed)
};

/// Refining processing unit specification.
struct RefiningUnit {
  std::string name;              ///< Unit name (e.g. "CDU", "VDU", "CCR", "FCC", "DHDS")
  double capacity_kbpd = 100.0;  ///< Maximum throughput capacity (kbpd)
  double min_throughput_kbpd = 0.0; ///< Minimum technical operating rate (turndown)
  double operating_cost_per_bbl = 1.50; ///< Variable operating cost ($/bbl feed)
  bool requires_binary_switch = false;  ///< If true, models minimum turndown with binary switch
};

/// Finished product specification and blending requirements.
struct ProductSpec {
  std::string name;              ///< Product pool (e.g., "LPG", "Gasoline", "Jet_A1", "Diesel_BS6", "Fuel_Oil")
  double selling_price_per_bbl = 90.0; ///< Realized product market value ($/bbl)
  double min_demand_kbpd = 10.0; ///< Mandatory contract commitment (kbpd)
  double max_demand_kbpd = 80.0; ///< Maximum market absorption limit (kbpd)

  // Product quality criteria
  double min_octane_ron = 0.0;   ///< Minimum Research Octane Number (for Gasoline)
  double max_sulfur_ppm = 1e6;   ///< Maximum allowable sulfur (parts per million)
  double min_cetane_number = 0.0;///< Minimum cetane index (for Diesel)
};

/// What-If scenario classification.
enum class ScenarioType {
  kBaseline = 0,         ///< Normal supply, demand, and standard unit capacities
  kHighDemand = 1,       ///< +25% Surge in product market demand
  kLimitedCrude = 2,     ///< Sweet crude disruption (-70% low-sulfur crude availability)
  kUnitConstraint = 3,   ///< Unplanned turnaround (-50% FCC unit capacity)
  kQualityConstraint = 4,///< Ultra-stringent quality standards (Euro-VI / BS-VI diesel & 95 RON)
  kInfeasibleDemand = 5  ///< Stress test: demand exceeding physical refinery distillation ceiling
};

[[nodiscard]] constexpr std::string_view to_string(ScenarioType type) noexcept {
  switch (type) {
    case ScenarioType::kBaseline: return "Baseline";
    case ScenarioType::kHighDemand: return "High Demand (+25%)";
    case ScenarioType::kLimitedCrude: return "Limited Sweet Crude (-70%)";
    case ScenarioType::kUnitConstraint: return "Unit Constraint (FCC -50%)";
    case ScenarioType::kQualityConstraint: return "Tighter Quality Spec";
    case ScenarioType::kInfeasibleDemand: return "Infeasible Demand Stress Test";
  }
  return "Unknown Scenario";
}

/// Complete configuration describing an industrial scenario.
struct ScenarioConfig {
  ScenarioType type = ScenarioType::kBaseline;
  std::string name = "Baseline Scenario";
  std::string description = "Normal operation under standard synthetic crude basket and unit availability.";

  std::vector<CrudeGrade> crudes;
  std::vector<RefiningUnit> units;
  std::vector<ProductSpec> products;

  bool enable_discrete_milp = false;     ///< Use binary unit activation constraints
  double quadratic_penalty_weight = 0.0;  ///< Use convex QP penalty on demand swings
};

/// Key Performance Indicators extracted from the solved refinery optimization.
struct RefineryKPIs {
  double total_gross_revenue_k_usd = 0.0;     ///< Total revenue ($1000/day)
  double total_crude_cost_k_usd = 0.0;        ///< Total crude feed procurement ($1000/day)
  double total_operating_cost_k_usd = 0.0;    ///< Total unit utility and catalyst costs ($1000/day)
  double net_operating_margin_k_usd = 0.0;    ///< Net profit / operating margin ($1000/day)
  double net_margin_per_bbl = 0.0;            ///< Gross refining margin (GRM) in $/bbl crude processed

  double total_crude_processed_kbpd = 0.0;    ///< Total crude distillation throughput
  double total_products_produced_kbpd = 0.0;  ///< Total finished product yield

  std::map<std::string, double> crude_allocation_kbpd;   ///< Allocation per crude grade
  std::map<std::string, double> crude_utilization_pct;   ///< % of max availability used
  std::map<std::string, double> unit_throughput_kbpd;    ///< Throughput for each refining unit
  std::map<std::string, double> unit_utilization_pct;    ///< % of rated capacity utilized
  std::map<std::string, double> product_production_kbpd; ///< Production rate per finished product
  std::map<std::string, double> demand_fulfillment_pct;  ///< Production relative to max demand

  std::vector<std::string> binding_bottlenecks;          ///< Active capacity / quality constraints

  // Execution & Verification metrics
  DeviceBackend backend_used = DeviceBackend::kCpu;
  double solve_seconds = 0.0;
  VerificationStatus verification_status = VerificationStatus::kNotApplicable;
  double max_residual = 0.0;
  bool is_verified_trustworthy = false;
};

/// Structured outcome of a digital twin run.
struct RefineryResult {
  ScenarioConfig config;
  Solution solver_solution;
  VerificationReport verification_report;
  RefineryKPIs kpis;
  std::string summary_text;
};

/// Comparative analytics between a baseline plan and a what-if scenario.
struct ScenarioComparison {
  std::string baseline_name;
  std::string scenario_name;

  double net_margin_delta_k_usd = 0.0;
  double net_margin_delta_pct = 0.0;
  double crude_processed_delta_kbpd = 0.0;
  double production_delta_kbpd = 0.0;

  std::map<std::string, double> product_production_deltas;
  std::map<std::string, double> unit_utilization_deltas;

  std::vector<std::string> new_active_bottlenecks;
  std::vector<std::string> relieved_bottlenecks;

  std::string comparative_analysis_report;
};

/// The Refinery Optimization Digital Twin.
class RefineryTwin {
 public:
  RefineryTwin() = default;

  /// Generate standard default synthetic configuration for any supported scenario.
  [[nodiscard]] static ScenarioConfig create_scenario(ScenarioType type);

  /// Formulate the optimization problem into a SANKHYA Model.
  [[nodiscard]] Model build_optimization_model(const ScenarioConfig& config) const;

  /// Execute end-to-end digital twin:
  ///   1. Build Model
  ///   2. Adaptive backend selection
  ///   3. Solve via SANKHYA
  ///   4. Independent verification via Trust Layer
  ///   5. Compute industrial KPIs
  [[nodiscard]] RefineryResult run_scenario(const ScenarioConfig& config,
                                            const Options& options = Options()) const;

  /// Perform what-if differential analysis between two solved results.
  [[nodiscard]] static ScenarioComparison compare_scenarios(const RefineryResult& baseline,
                                                            const RefineryResult& scenario);
};

}  // namespace sankhya::refinery
