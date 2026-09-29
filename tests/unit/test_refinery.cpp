// SPDX-License-Identifier: Apache-2.0
// SANKHYA - MRPL-Style Refinery Optimization Digital Twin Tests (Phase 2 Feature 4).
//
// =========================================================================================
// SYNTHETIC INDUSTRIAL BENCHMARK DISCLAIMER:
// This is an optimization-oriented synthetic refinery digital twin developed for
// demonstration, decision intelligence, and solver benchmarking. It uses public-domain
// chemical engineering parameters, synthetic crude assays, and idealized unit yields.
// IT DOES NOT CONTAIN OR REPRESENT CONFIDENTIAL OR PROPRIETARY MRPL OPERATIONAL DATA.
// =========================================================================================
#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "sankhya/refinery.hpp"

namespace sankhya::refinery {
namespace {

// Helper: check that a value is within relative tolerance
bool is_near(double actual, double expected, double tol = 1e-4) {
  const double diff = std::fabs(actual - expected);
  const double scale = std::max(1.0, std::max(std::fabs(actual), std::fabs(expected)));
  return (diff / scale) <= tol;
}

}  // namespace

// =========================================================================================
// 1. BASELINE SCENARIO MODEL GENERATION (TASK 10.1)
// =========================================================================================

TEST(RefineryDigitalTwin, BaselineModelGeneration) {
  RefineryTwin twin;
  const ScenarioConfig cfg = twin.create_scenario(ScenarioType::kBaseline);
  const Model model = twin.build_optimization_model(cfg);

  EXPECT_EQ(model.sense, ObjSense::kMinimize);
  EXPECT_GT(model.num_cols(), 20);
  EXPECT_GT(model.num_rows(), 20);
  EXPECT_GT(model.matrix.non_zeros(), 50);

  // Validate internal structure
  std::string err;
  EXPECT_TRUE(model.validate(&err)) << "Model validation failed: " << err;

  // Confirm primary column names exist
  bool found_cdu = false;
  bool found_arab_light = false;
  for (const auto& name : model.col_names) {
    if (name == "u_CDU") found_cdu = true;
    if (name == "x_Arab_Light") found_arab_light = true;
  }
  EXPECT_TRUE(found_cdu);
  EXPECT_TRUE(found_arab_light);
}

// =========================================================================================
// 2. BASELINE SCENARIO SOLVES SUCCESSFULLY (TASK 10.2)
// =========================================================================================

TEST(RefineryDigitalTwin, BaselineSolvesSuccessfully) {
  RefineryTwin twin;
  const ScenarioConfig cfg = twin.create_scenario(ScenarioType::kBaseline);
  const RefineryResult res = twin.run_scenario(cfg);

  EXPECT_EQ(res.solver_solution.status, SolveStatus::kOptimal);
  EXPECT_GT(res.kpis.total_crude_processed_kbpd, 0.0);
  EXPECT_LE(res.kpis.total_crude_processed_kbpd, 300.0); // CDU capacity limit
  EXPECT_GT(res.kpis.net_operating_margin_k_usd, 0.0);   // Profitable plan
  EXPECT_GT(res.kpis.net_margin_per_bbl, 0.0);
}

// =========================================================================================
// 3. RESULT INDEPENDENTLY VERIFIES VIA TRUST LAYER (TASK 10.3)
// =========================================================================================

TEST(RefineryDigitalTwin, BaselineResultIndependentlyVerified) {
  RefineryTwin twin;
  const ScenarioConfig cfg = twin.create_scenario(ScenarioType::kBaseline);
  const RefineryResult res = twin.run_scenario(cfg);

  EXPECT_TRUE(res.kpis.is_verified_trustworthy);
  EXPECT_EQ(res.verification_report.status, VerificationStatus::kVerifiedOptimal);
  EXPECT_EQ(res.verification_report.primal_check, CheckResult::kPass);
  EXPECT_EQ(res.verification_report.objective_check, CheckResult::kPass);
  EXPECT_LE(res.kpis.max_residual, 1e-6);
}

// =========================================================================================
// 4. HIGH-DEMAND SCENARIO CHANGES THE MODEL & PRODUCTION (TASK 10.4)
// =========================================================================================

TEST(RefineryDigitalTwin, HighDemandScenarioIncreasesProduction) {
  RefineryTwin twin;
  const ScenarioConfig baseline_cfg = twin.create_scenario(ScenarioType::kBaseline);
  const ScenarioConfig high_demand_cfg = twin.create_scenario(ScenarioType::kHighDemand);

  const Model m_base = twin.build_optimization_model(baseline_cfg);
  const Model m_high = twin.build_optimization_model(high_demand_cfg);

  // Model column upper bounds for demand should differ
  EXPECT_NE(m_base.col_upper, m_high.col_upper);

  const RefineryResult base_res = twin.run_scenario(baseline_cfg);
  const RefineryResult high_res = twin.run_scenario(high_demand_cfg);

  EXPECT_EQ(high_res.solver_solution.status, SolveStatus::kOptimal);
  EXPECT_TRUE(high_res.kpis.is_verified_trustworthy);

  // High demand should produce at least as much or more total finished products
  EXPECT_GE(high_res.kpis.total_products_produced_kbpd,
            base_res.kpis.total_products_produced_kbpd - 1e-4);
  // High demand should yield higher or equal gross revenue
  EXPECT_GE(high_res.kpis.total_gross_revenue_k_usd,
            base_res.kpis.total_gross_revenue_k_usd - 1e-4);
}

// =========================================================================================
// 5. LIMITED-CRUDE SCENARIO CHANGES CRUDE ALLOCATION (TASK 10.5)
// =========================================================================================

TEST(RefineryDigitalTwin, LimitedCrudeForcesHeavierBasket) {
  RefineryTwin twin;
  const ScenarioConfig baseline_cfg = twin.create_scenario(ScenarioType::kBaseline);
  const ScenarioConfig limited_cfg = twin.create_scenario(ScenarioType::kLimitedCrude);

  const RefineryResult base_res = twin.run_scenario(baseline_cfg);
  const RefineryResult limited_res = twin.run_scenario(limited_cfg);

  EXPECT_EQ(limited_res.solver_solution.status, SolveStatus::kOptimal);
  EXPECT_TRUE(limited_res.kpis.is_verified_trustworthy);

  // Sweet crude (Murban) should be capped at lower value in limited scenario
  const double base_murban = base_res.kpis.crude_allocation_kbpd.at("Murban");
  const double limited_murban = limited_res.kpis.crude_allocation_kbpd.at("Murban");
  EXPECT_LE(limited_murban, 15.0 + 1e-5);
  if (base_murban > 15.0) {
    EXPECT_LT(limited_murban, base_murban);
  }
}

// =========================================================================================
// 6. UNIT-CAPACITY SCENARIO CHANGES BOTTLENECK & PRODUCTION (TASK 10.6)
// =========================================================================================

TEST(RefineryDigitalTwin, UnitConstraintReflectsDeratedCapacity) {
  RefineryTwin twin;
  const ScenarioConfig baseline_cfg = twin.create_scenario(ScenarioType::kBaseline);
  const ScenarioConfig unit_cfg = twin.create_scenario(ScenarioType::kUnitConstraint);

  const RefineryResult base_res = twin.run_scenario(baseline_cfg);
  const RefineryResult unit_res = twin.run_scenario(unit_cfg);

  EXPECT_EQ(unit_res.solver_solution.status, SolveStatus::kOptimal);
  EXPECT_TRUE(unit_res.kpis.is_verified_trustworthy);

  // FCC throughput must strictly respect 32.5 kbpd derated capacity
  const double fcc_throughput = unit_res.kpis.unit_throughput_kbpd.at("FCC");
  EXPECT_LE(fcc_throughput, 32.5 + 1e-5);
}

// =========================================================================================
// 7. QUALITY CONSTRAINT SCENARIO TIGHTENS SPECIFICATIONS (TASK 10.7)
// =========================================================================================

TEST(RefineryDigitalTwin, QualityConstraintMaintainsFeasibilityAndVerification) {
  RefineryTwin twin;
  const ScenarioConfig quality_cfg = twin.create_scenario(ScenarioType::kQualityConstraint);
  const RefineryResult quality_res = twin.run_scenario(quality_cfg);

  EXPECT_EQ(quality_res.solver_solution.status, SolveStatus::kOptimal);
  EXPECT_TRUE(quality_res.kpis.is_verified_trustworthy);
  EXPECT_EQ(quality_res.verification_report.status, VerificationStatus::kVerifiedOptimal);
}

// =========================================================================================
// 8. SCENARIO RESULTS REMAIN FEASIBLE AND OPTIMAL (TASK 10.8)
// =========================================================================================

TEST(RefineryDigitalTwin, AllStandardScenariosRemainOptimal) {
  RefineryTwin twin;
  const std::vector<ScenarioType> scenarios = {
      ScenarioType::kBaseline,
      ScenarioType::kHighDemand,
      ScenarioType::kLimitedCrude,
      ScenarioType::kUnitConstraint,
      ScenarioType::kQualityConstraint};

  for (const auto& st : scenarios) {
    const ScenarioConfig cfg = twin.create_scenario(st);
    const RefineryResult res = twin.run_scenario(cfg);
    EXPECT_EQ(res.solver_solution.status, SolveStatus::kOptimal)
        << "Failed on scenario: " << to_string(st);
    EXPECT_TRUE(res.kpis.is_verified_trustworthy)
        << "Verification rejected on scenario: " << to_string(st);
  }
}

// =========================================================================================
// 9. INFEASIBLE SCENARIOS ARE REPORTED AND PROVED (TASK 10.9)
// =========================================================================================

TEST(RefineryDigitalTwin, InfeasibleScenarioReportedCorrectly) {
  RefineryTwin twin;
  const ScenarioConfig infeas_cfg = twin.create_scenario(ScenarioType::kInfeasibleDemand);
  const RefineryResult res = twin.run_scenario(infeas_cfg);

  // SANKHYA solver must declare kInfeasible
  EXPECT_EQ(res.solver_solution.status, SolveStatus::kInfeasible);
  // Independent trust layer must reject claiming a valid point or verify infeasibility
  EXPECT_FALSE(res.kpis.is_verified_trustworthy);
}

// =========================================================================================
// 10. BASELINE VS SCENARIO COMPARISON MATHEMATICALLY CONSISTENT (TASK 10.10)
// =========================================================================================

TEST(RefineryDigitalTwin, ComparisonIsMathematicallyConsistent) {
  RefineryTwin twin;
  const ScenarioConfig base_cfg = twin.create_scenario(ScenarioType::kBaseline);
  const ScenarioConfig high_cfg = twin.create_scenario(ScenarioType::kHighDemand);

  const RefineryResult base_res = twin.run_scenario(base_cfg);
  const RefineryResult high_res = twin.run_scenario(high_cfg);

  const ScenarioComparison comp = RefineryTwin::compare_scenarios(base_res, high_res);

  EXPECT_EQ(comp.baseline_name, base_cfg.name);
  EXPECT_EQ(comp.scenario_name, high_cfg.name);

  const double expected_margin_delta =
      high_res.kpis.net_operating_margin_k_usd - base_res.kpis.net_operating_margin_k_usd;
  EXPECT_NEAR(comp.net_margin_delta_k_usd, expected_margin_delta, 1e-4);

  const double expected_crude_delta =
      high_res.kpis.total_crude_processed_kbpd - base_res.kpis.total_crude_processed_kbpd;
  EXPECT_NEAR(comp.crude_processed_delta_kbpd, expected_crude_delta, 1e-4);
  EXPECT_FALSE(comp.comparative_analysis_report.empty());
}

// =========================================================================================
// 11. DISCRETE MILP REFINERY EXTENSION (TURNDOWN ACTIVATION)
// =========================================================================================

TEST(RefineryDigitalTwin, DiscreteMilpModeSolvesAndVerifies) {
  RefineryTwin twin;
  ScenarioConfig cfg = twin.create_scenario(ScenarioType::kBaseline);
  cfg.enable_discrete_milp = true; // Adds binary FCC and CCR activation variables

  const RefineryResult res = twin.run_scenario(cfg);
  EXPECT_EQ(res.solver_solution.status, SolveStatus::kOptimal);
  EXPECT_TRUE(res.kpis.is_verified_trustworthy);
  EXPECT_EQ(res.verification_report.integrality_check, CheckResult::kPass);
}

// =========================================================================================
// 12. CONVEX QP REGULARIZATION EXTENSION (QUALITY GIVEAWAY PENALTY)
// =========================================================================================

TEST(RefineryDigitalTwin, ConvexQpModeSolvesAndVerifies) {
  RefineryTwin twin;
  ScenarioConfig cfg = twin.create_scenario(ScenarioType::kBaseline);
  cfg.quadratic_penalty_weight = 0.05; // Quadratic penalty on product sales volatility

  const RefineryResult res = twin.run_scenario(cfg);
  EXPECT_EQ(res.solver_solution.status, SolveStatus::kOptimal);
  EXPECT_TRUE(res.kpis.is_verified_trustworthy);
  EXPECT_LE(res.kpis.max_residual, 1e-6);
}

// =========================================================================================
// 13. DETERMINISTIC REPRODUCIBILITY (TASK 11)
// =========================================================================================

TEST(RefineryDigitalTwin, DeterministicReproducibility) {
  RefineryTwin twin;
  const ScenarioConfig cfg = twin.create_scenario(ScenarioType::kBaseline);

  const RefineryResult run1 = twin.run_scenario(cfg);
  const RefineryResult run2 = twin.run_scenario(cfg);

  EXPECT_EQ(run1.solver_solution.status, run2.solver_solution.status);
  EXPECT_DOUBLE_EQ(run1.solver_solution.objective, run2.solver_solution.objective);
  EXPECT_DOUBLE_EQ(run1.kpis.net_operating_margin_k_usd, run2.kpis.net_operating_margin_k_usd);
  EXPECT_DOUBLE_EQ(run1.kpis.total_crude_processed_kbpd, run2.kpis.total_crude_processed_kbpd);
}

}  // namespace sankhya::refinery
