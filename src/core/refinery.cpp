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
#include "sankhya/refinery.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

#include <fmt/format.h>

#include "sankhya/solve.hpp"

namespace sankhya::refinery {

ScenarioConfig RefineryTwin::create_scenario(ScenarioType type) {
  ScenarioConfig cfg;
  cfg.type = type;

  // 1. Synthetic Crude Basket (public domain crude archetypes)
  CrudeGrade arab_light;
  arab_light.name = "Arab_Light";
  arab_light.api_gravity = 33.4;
  arab_light.sulfur_wt_pct = 1.85;
  arab_light.price_per_bbl = 78.0;
  arab_light.max_avail_kbpd = 60.0;
  arab_light.yield_lpg = 0.03;
  arab_light.yield_light_naphtha = 0.12;
  arab_light.yield_heavy_naphtha = 0.15;
  arab_light.yield_kerosene = 0.14;
  arab_light.yield_lgo = 0.26;
  arab_light.yield_heavy_residue = 0.30;

  CrudeGrade arab_heavy;
  arab_heavy.name = "Arab_Heavy";
  arab_heavy.api_gravity = 27.0;
  arab_heavy.sulfur_wt_pct = 2.90;
  arab_heavy.price_per_bbl = 72.0;
  arab_heavy.max_avail_kbpd = 80.0;
  arab_heavy.yield_lpg = 0.02;
  arab_heavy.yield_light_naphtha = 0.08;
  arab_heavy.yield_heavy_naphtha = 0.11;
  arab_heavy.yield_kerosene = 0.10;
  arab_heavy.yield_lgo = 0.23;
  arab_heavy.yield_heavy_residue = 0.46;

  CrudeGrade kuwait;
  kuwait.name = "Kuwait_Export";
  kuwait.api_gravity = 30.2;
  kuwait.sulfur_wt_pct = 2.40;
  kuwait.price_per_bbl = 74.0;
  kuwait.max_avail_kbpd = 60.0;
  kuwait.yield_lpg = 0.025;
  kuwait.yield_light_naphtha = 0.10;
  kuwait.yield_heavy_naphtha = 0.13;
  kuwait.yield_kerosene = 0.12;
  kuwait.yield_lgo = 0.25;
  kuwait.yield_heavy_residue = 0.375;

  CrudeGrade murban;
  murban.name = "Murban";
  murban.api_gravity = 40.2;
  murban.sulfur_wt_pct = 0.75;
  murban.price_per_bbl = 82.0;
  murban.max_avail_kbpd = 50.0;
  murban.yield_lpg = 0.04;
  murban.yield_light_naphtha = 0.16;
  murban.yield_heavy_naphtha = 0.19;
  murban.yield_kerosene = 0.17;
  murban.yield_lgo = 0.28;
  murban.yield_heavy_residue = 0.16;

  CrudeGrade bonny;
  bonny.name = "Bonny_Light";
  bonny.api_gravity = 35.3;
  bonny.sulfur_wt_pct = 0.15;
  bonny.price_per_bbl = 84.0;
  bonny.max_avail_kbpd = 50.0;
  bonny.yield_lpg = 0.035;
  bonny.yield_light_naphtha = 0.15;
  bonny.yield_heavy_naphtha = 0.17;
  bonny.yield_kerosene = 0.16;
  bonny.yield_lgo = 0.30;
  bonny.yield_heavy_residue = 0.185;

  CrudeGrade domestic_sweet;
  domestic_sweet.name = "Domestic_Sweet";
  domestic_sweet.api_gravity = 38.5;
  domestic_sweet.sulfur_wt_pct = 0.12;
  domestic_sweet.price_per_bbl = 83.0;
  domestic_sweet.max_avail_kbpd = 40.0;
  domestic_sweet.yield_lpg = 0.04;
  domestic_sweet.yield_light_naphtha = 0.16;
  domestic_sweet.yield_heavy_naphtha = 0.18;
  domestic_sweet.yield_kerosene = 0.17;
  domestic_sweet.yield_lgo = 0.29;
  domestic_sweet.yield_heavy_residue = 0.16;

  cfg.crudes = {arab_light, arab_heavy, kuwait, murban, bonny, domestic_sweet};

  // 2. Refining Processing Units
  RefiningUnit cdu{"CDU", 300.0, 100.0, 1.20, false};
  RefiningUnit vdu{"VDU", 140.0, 40.0, 1.50, false};
  RefiningUnit ccr{"CCR", 45.0, 10.0, 3.50, false};
  RefiningUnit fcc{"FCC", 65.0, 20.0, 2.80, false};
  RefiningUnit dhds{"DHDS", 110.0, 30.0, 2.20, false};

  cfg.units = {cdu, vdu, ccr, fcc, dhds};

  // 3. Finished Products and Specifications
  ProductSpec lpg{"LPG", 65.0, 10.0, 45.0, 0.0, 1e6, 0.0};
  ProductSpec ms{"MS_Gasoline", 98.0, 35.0, 90.0, 91.0, 10.0, 0.0};
  ProductSpec atf{"ATF_Jet", 94.0, 20.0, 60.0, 0.0, 1500.0, 0.0};
  ProductSpec hsd{"HSD_Diesel", 92.0, 80.0, 180.0, 0.0, 10.0, 51.0};
  ProductSpec fo{"Fuel_Oil", 58.0, 15.0, 70.0, 0.0, 35000.0, 0.0};

  cfg.products = {lpg, ms, atf, hsd, fo};

  // 4. Scenario-Specific Adjustments
  switch (type) {
    case ScenarioType::kBaseline:
      cfg.name = "Baseline Refinery Scenario";
      cfg.description = "Normal operating baseline with 300 kbpd nominal CDU capacity.";
      break;

    case ScenarioType::kHighDemand:
      cfg.name = "High Demand Surge Scenario";
      cfg.description = "High domestic transport demand: +25% gasoline & +22% diesel requirements.";
      for (auto& prod : cfg.products) {
        if (prod.name == "MS_Gasoline") {
          prod.min_demand_kbpd = 45.0;
          prod.max_demand_kbpd = 110.0;
        } else if (prod.name == "HSD_Diesel") {
          prod.min_demand_kbpd = 100.0;
          prod.max_demand_kbpd = 220.0;
        }
      }
      break;

    case ScenarioType::kLimitedCrude:
      cfg.name = "Limited Sweet Crude Disruption";
      cfg.description = "Geopolitical/freight disruption: Sweet crude availability slashed by 70%.";
      for (auto& crude : cfg.crudes) {
        if (crude.name == "Murban") crude.max_avail_kbpd = 15.0;
        if (crude.name == "Bonny_Light") crude.max_avail_kbpd = 15.0;
        if (crude.name == "Domestic_Sweet") crude.max_avail_kbpd = 12.0;
      }
      break;

    case ScenarioType::kUnitConstraint:
      cfg.name = "FCC Turnaround Bottleneck";
      cfg.description = "FCC unit turnaround: capacity derated by 50% (65 -> 32.5 kbpd).";
      for (auto& unit : cfg.units) {
        if (unit.name == "FCC") unit.capacity_kbpd = 32.5;
      }
      break;

    case ScenarioType::kQualityConstraint:
      cfg.name = "Ultra-Stringent Quality Spec";
      cfg.description = "Tighter specifications: MS octane raised to 95 RON, diesel sulfur capped at 8 ppm.";
      for (auto& prod : cfg.products) {
        if (prod.name == "MS_Gasoline") prod.min_octane_ron = 95.0;
        if (prod.name == "HSD_Diesel") prod.max_sulfur_ppm = 8.0;
      }
      break;

    case ScenarioType::kInfeasibleDemand:
      cfg.name = "Infeasible Demand Stress Test";
      cfg.description = "Mandatory contract demand for diesel set to 350 kbpd, exceeding 300 kbpd CDU ceiling.";
      for (auto& prod : cfg.products) {
        if (prod.name == "HSD_Diesel") {
          prod.min_demand_kbpd = 350.0;
          prod.max_demand_kbpd = 400.0;
        }
      }
      break;
  }

  return cfg;
}

Model RefineryTwin::build_optimization_model(const ScenarioConfig& config) const {
  Model model;
  model.name = "mrpl_synthetic_refinery_" + std::string(to_string(config.type));
  model.sense = ObjSense::kMinimize; // Minimize Net Cost = -Net Margin

  // Column Index Mapping
  // 1. Crudes: x_k (size = config.crudes.size())
  // 2. CDU Throughput: u_cdu
  // 3. Primary CDU Cuts: cdu_lpg, cdu_ln, cdu_hn, cdu_kero, cdu_lgo, cdu_ar
  // 4. Secondary Unit Feeds: u_vdu, u_ccr, u_fcc, u_dhds
  // 5. Secondary Unit Yields: vdu_vgo, vdu_vr, ccr_ref, ccr_lpg, fcc_gas, fcc_lco, fcc_lpg, dhds_diesel
  // 6. Blending streams: b_ln_ms, b_ref_ms, b_fccgas_ms, b_kero_atf, b_dhds_hsd, b_lgo_hsd, b_vr_fo, b_ar_fo, b_vgo_fo
  // 7. Product Sales: y_lpg, y_ms, y_atf, y_hsd, y_fo
  // 8. (Optional) Binary switches if config.enable_discrete_milp: z_fcc, z_ccr

  std::vector<std::string> col_names;
  std::vector<double> col_cost;
  std::vector<double> col_lower;
  std::vector<double> col_upper;
  std::vector<bool> is_integer;

  auto add_column = [&](const std::string& name, double cost, double lo, double hi, bool integer = false) -> Index {
    const auto idx = static_cast<Index>(col_names.size());
    col_names.push_back(name);
    col_cost.push_back(cost);
    col_lower.push_back(lo);
    col_upper.push_back(hi);
    is_integer.push_back(integer);
    return idx;
  };

  // 1. Crude columns
  std::vector<Index> col_crude(config.crudes.size());
  for (std::size_t i = 0; i < config.crudes.size(); ++i) {
    const auto& c = config.crudes[i];
    col_crude[i] = add_column("x_" + c.name, c.price_per_bbl, 0.0, c.max_avail_kbpd);
  }

  // 2. CDU Throughput
  double cdu_cap = 300.0;
  double cdu_cost = 1.20;
  for (const auto& u : config.units) {
    if (u.name == "CDU") { cdu_cap = u.capacity_kbpd; cdu_cost = u.operating_cost_per_bbl; break; }
  }
  const Index col_u_cdu = add_column("u_CDU", cdu_cost, 0.0, cdu_cap);

  // 3. Primary CDU Cuts
  const Index col_cdu_lpg = add_column("cdu_lpg", 0.0, 0.0, kInfinity);
  const Index col_cdu_ln = add_column("cdu_ln", 0.0, 0.0, kInfinity);
  const Index col_cdu_hn = add_column("cdu_hn", 0.0, 0.0, kInfinity);
  const Index col_cdu_kero = add_column("cdu_kero", 0.0, 0.0, kInfinity);
  const Index col_cdu_lgo = add_column("cdu_lgo", 0.0, 0.0, kInfinity);
  const Index col_cdu_ar = add_column("cdu_ar", 0.0, 0.0, kInfinity);

  // 4. Secondary Unit Feeds
  double vdu_cap = 140.0, vdu_cost = 1.50;
  double ccr_cap = 45.0, ccr_cost = 3.50;
  double fcc_cap = 65.0, fcc_cost = 2.80;
  double dhds_cap = 110.0, dhds_cost = 2.20;

  for (const auto& u : config.units) {
    if (u.name == "VDU") { vdu_cap = u.capacity_kbpd; vdu_cost = u.operating_cost_per_bbl; }
    if (u.name == "CCR") { ccr_cap = u.capacity_kbpd; ccr_cost = u.operating_cost_per_bbl; }
    if (u.name == "FCC") { fcc_cap = u.capacity_kbpd; fcc_cost = u.operating_cost_per_bbl; }
    if (u.name == "DHDS") { dhds_cap = u.capacity_kbpd; dhds_cost = u.operating_cost_per_bbl; }
  }

  const Index col_u_vdu = add_column("u_VDU", vdu_cost, 0.0, vdu_cap);
  const Index col_u_ccr = add_column("u_CCR", ccr_cost, 0.0, ccr_cap);
  const Index col_u_fcc = add_column("u_FCC", fcc_cost, 0.0, fcc_cap);
  const Index col_u_dhds = add_column("u_DHDS", dhds_cost, 0.0, dhds_cap);

  // 5. Secondary Unit Yields
  const Index col_vdu_vgo = add_column("vdu_vgo", 0.0, 0.0, kInfinity);
  const Index col_vdu_vr = add_column("vdu_vr", 0.0, 0.0, kInfinity);
  const Index col_ccr_ref = add_column("ccr_reformate", 0.0, 0.0, kInfinity);
  const Index col_ccr_lpg = add_column("ccr_lpg", 0.0, 0.0, kInfinity);
  const Index col_fcc_gas = add_column("fcc_gasoline", 0.0, 0.0, kInfinity);
  const Index col_fcc_lco = add_column("fcc_lco", 0.0, 0.0, kInfinity);
  const Index col_fcc_lpg = add_column("fcc_lpg", 0.0, 0.0, kInfinity);
  const Index col_dhds_diesel = add_column("dhds_diesel", 0.0, 0.0, kInfinity);

  // 6. Blending stream allocations
  const Index col_b_ln_ms = add_column("b_ln_ms", 0.0, 0.0, kInfinity);
  const Index col_b_ref_ms = add_column("b_ref_ms", 0.0, 0.0, kInfinity);
  const Index col_b_fccgas_ms = add_column("b_fccgas_ms", 0.0, 0.0, kInfinity);
  const Index col_b_kero_atf = add_column("b_kero_atf", 0.0, 0.0, kInfinity);
  const Index col_b_dhds_hsd = add_column("b_dhds_hsd", 0.0, 0.0, kInfinity);
  const Index col_b_lgo_hsd = add_column("b_lgo_hsd", 0.0, 0.0, kInfinity);
  const Index col_b_vr_fo = add_column("b_vr_fo", 0.0, 0.0, kInfinity);
  const Index col_b_ar_fo = add_column("b_ar_fo", 0.0, 0.0, kInfinity);
  const Index col_b_vgo_fo = add_column("b_vgo_fo", 0.0, 0.0, kInfinity);

  // 7. Product Sales Columns (-selling_price in minimize objective)
  std::map<std::string, Index> col_products;
  for (const auto& p : config.products) {
    col_products[p.name] = add_column("y_" + p.name, -p.selling_price_per_bbl,
                                      p.min_demand_kbpd, p.max_demand_kbpd);
  }

  // 8. Optional Discrete MILP Switches
  Index col_z_fcc = -1;
  Index col_z_ccr = -1;
  if (config.enable_discrete_milp) {
    col_z_fcc = add_column("z_fcc", 10.0, 0.0, 1.0, true); // $10k fixed operating charge
    col_z_ccr = add_column("z_ccr", 8.0, 0.0, 1.0, true);  // $8k fixed operating charge
  }

  // =======================================================================================
  // Rows & Constraints Setup
  // =======================================================================================
  std::vector<std::string> row_names;
  std::vector<double> row_lower;
  std::vector<double> row_upper;
  struct Entry { Index row; Index col; double val; };
  std::vector<Entry> entries;

  auto add_row = [&](const std::string& name, double lo, double hi) -> Index {
    const auto r = static_cast<Index>(row_names.size());
    row_names.push_back(name);
    row_lower.push_back(lo);
    row_upper.push_back(hi);
    return r;
  };

  // Row: CDU throughput definition: u_CDU - sum(x_k) = 0
  const Index r_cdu_def = add_row("CDU_balance", 0.0, 0.0);
  entries.push_back({r_cdu_def, col_u_cdu, 1.0});
  for (const auto& c_idx : col_crude) {
    entries.push_back({r_cdu_def, c_idx, -1.0});
  }

  // Rows: Primary CDU cut yield equations: cdu_cut - sum(Y_k * x_k) = 0
  const Index r_cdu_lpg = add_row("CDU_yield_lpg", 0.0, 0.0);
  entries.push_back({r_cdu_lpg, col_cdu_lpg, 1.0});
  for (std::size_t i = 0; i < config.crudes.size(); ++i) {
    entries.push_back({r_cdu_lpg, col_crude[i], -config.crudes[i].yield_lpg});
  }

  const Index r_cdu_ln = add_row("CDU_yield_ln", 0.0, 0.0);
  entries.push_back({r_cdu_ln, col_cdu_ln, 1.0});
  for (std::size_t i = 0; i < config.crudes.size(); ++i) {
    entries.push_back({r_cdu_ln, col_crude[i], -config.crudes[i].yield_light_naphtha});
  }

  const Index r_cdu_hn = add_row("CDU_yield_hn", 0.0, 0.0);
  entries.push_back({r_cdu_hn, col_cdu_hn, 1.0});
  for (std::size_t i = 0; i < config.crudes.size(); ++i) {
    entries.push_back({r_cdu_hn, col_crude[i], -config.crudes[i].yield_heavy_naphtha});
  }

  const Index r_cdu_kero = add_row("CDU_yield_kero", 0.0, 0.0);
  entries.push_back({r_cdu_kero, col_cdu_kero, 1.0});
  for (std::size_t i = 0; i < config.crudes.size(); ++i) {
    entries.push_back({r_cdu_kero, col_crude[i], -config.crudes[i].yield_kerosene});
  }

  const Index r_cdu_lgo = add_row("CDU_yield_lgo", 0.0, 0.0);
  entries.push_back({r_cdu_lgo, col_cdu_lgo, 1.0});
  for (std::size_t i = 0; i < config.crudes.size(); ++i) {
    entries.push_back({r_cdu_lgo, col_crude[i], -config.crudes[i].yield_lgo});
  }

  const Index r_cdu_ar = add_row("CDU_yield_ar", 0.0, 0.0);
  entries.push_back({r_cdu_ar, col_cdu_ar, 1.0});
  for (std::size_t i = 0; i < config.crudes.size(); ++i) {
    entries.push_back({r_cdu_ar, col_crude[i], -config.crudes[i].yield_heavy_residue});
  }

  // Row: CCR Feed balance: u_CCR <= cdu_hn (u_CCR - cdu_hn <= 0)
  const Index r_ccr_feed = add_row("CCR_feed_limit", -kInfinity, 0.0);
  entries.push_back({r_ccr_feed, col_u_ccr, 1.0});
  entries.push_back({r_ccr_feed, col_cdu_hn, -1.0});

  // Rows: CCR Yields: reformate = 0.88 * u_CCR, ccr_lpg = 0.08 * u_CCR
  const Index r_ccr_ref = add_row("CCR_yield_reformate", 0.0, 0.0);
  entries.push_back({r_ccr_ref, col_ccr_ref, 1.0});
  entries.push_back({r_ccr_ref, col_u_ccr, -0.88});

  const Index r_ccr_lpg = add_row("CCR_yield_lpg", 0.0, 0.0);
  entries.push_back({r_ccr_lpg, col_ccr_lpg, 1.0});
  entries.push_back({r_ccr_lpg, col_u_ccr, -0.08});

  // Row: VDU Feed balance: u_VDU + b_ar_fo - cdu_ar = 0
  const Index r_vdu_feed = add_row("VDU_feed_balance", 0.0, 0.0);
  entries.push_back({r_vdu_feed, col_u_vdu, 1.0});
  entries.push_back({r_vdu_feed, col_b_ar_fo, 1.0});
  entries.push_back({r_vdu_feed, col_cdu_ar, -1.0});

  // Rows: VDU Yields: vgo = 0.55 * u_VDU, vr = 0.45 * u_VDU
  const Index r_vdu_vgo = add_row("VDU_yield_vgo", 0.0, 0.0);
  entries.push_back({r_vdu_vgo, col_vdu_vgo, 1.0});
  entries.push_back({r_vdu_vgo, col_u_vdu, -0.55});

  const Index r_vdu_vr = add_row("VDU_yield_vr", 0.0, 0.0);
  entries.push_back({r_vdu_vr, col_vdu_vr, 1.0});
  entries.push_back({r_vdu_vr, col_u_vdu, -0.45});

  // Row: FCC Feed balance: u_FCC + b_vgo_fo - vdu_vgo = 0
  const Index r_fcc_feed = add_row("FCC_feed_balance", 0.0, 0.0);
  entries.push_back({r_fcc_feed, col_u_fcc, 1.0});
  entries.push_back({r_fcc_feed, col_b_vgo_fo, 1.0});
  entries.push_back({r_fcc_feed, col_vdu_vgo, -1.0});

  // Rows: FCC Yields: gas = 0.52 * u_FCC, lco = 0.24 * u_FCC, lpg = 0.16 * u_FCC
  const Index r_fcc_gas = add_row("FCC_yield_gas", 0.0, 0.0);
  entries.push_back({r_fcc_gas, col_fcc_gas, 1.0});
  entries.push_back({r_fcc_gas, col_u_fcc, -0.52});

  const Index r_fcc_lco = add_row("FCC_yield_lco", 0.0, 0.0);
  entries.push_back({r_fcc_lco, col_fcc_lco, 1.0});
  entries.push_back({r_fcc_lco, col_u_fcc, -0.24});

  const Index r_fcc_lpg = add_row("FCC_yield_lpg", 0.0, 0.0);
  entries.push_back({r_fcc_lpg, col_fcc_lpg, 1.0});
  entries.push_back({r_fcc_lpg, col_u_fcc, -0.16});

  // Row: DHDS Feed balance: u_DHDS + b_lgo_hsd - cdu_lgo - col_fcc_lco = 0
  const Index r_dhds_feed = add_row("DHDS_feed_balance", 0.0, 0.0);
  entries.push_back({r_dhds_feed, col_u_dhds, 1.0});
  entries.push_back({r_dhds_feed, col_b_lgo_hsd, 1.0});
  entries.push_back({r_dhds_feed, col_cdu_lgo, -1.0});
  entries.push_back({r_dhds_feed, col_fcc_lco, -1.0});

  // Row: DHDS Yield: dhds_diesel = 0.98 * u_DHDS
  const Index r_dhds_diesel = add_row("DHDS_yield_diesel", 0.0, 0.0);
  entries.push_back({r_dhds_diesel, col_dhds_diesel, 1.0});
  entries.push_back({r_dhds_diesel, col_u_dhds, -0.98});

  // =======================================================================================
  // Blending Pool Balance Equations
  // =======================================================================================
  // 1. LPG Pool: y_LPG - (cdu_lpg + ccr_lpg + fcc_lpg) = 0
  const Index r_pool_lpg = add_row("Pool_LPG", 0.0, 0.0);
  entries.push_back({r_pool_lpg, col_products["LPG"], 1.0});
  entries.push_back({r_pool_lpg, col_cdu_lpg, -1.0});
  entries.push_back({r_pool_lpg, col_ccr_lpg, -1.0});
  entries.push_back({r_pool_lpg, col_fcc_lpg, -1.0});

  // 2. MS Gasoline Pool: y_MS - (b_ln_ms + b_ref_ms + b_fccgas_ms) = 0
  const Index r_pool_ms = add_row("Pool_Gasoline", 0.0, 0.0);
  entries.push_back({r_pool_ms, col_products["MS_Gasoline"], 1.0});
  entries.push_back({r_pool_ms, col_b_ln_ms, -1.0});
  entries.push_back({r_pool_ms, col_b_ref_ms, -1.0});
  entries.push_back({r_pool_ms, col_b_fccgas_ms, -1.0});

  // Stream bounds for MS:
  const Index r_ln_avail = add_row("LN_availability", -kInfinity, 0.0);
  entries.push_back({r_ln_avail, col_b_ln_ms, 1.0});
  entries.push_back({r_ln_avail, col_cdu_ln, -1.0});

  const Index r_ref_avail = add_row("Reformate_avail", -kInfinity, 0.0);
  entries.push_back({r_ref_avail, col_b_ref_ms, 1.0});
  entries.push_back({r_ref_avail, col_ccr_ref, -1.0});

  const Index r_fccgas_avail = add_row("FCC_gas_avail", -kInfinity, 0.0);
  entries.push_back({r_fccgas_avail, col_b_fccgas_ms, 1.0});
  entries.push_back({r_fccgas_avail, col_fcc_gas, -1.0});

  // 3. ATF Jet Pool: y_ATF - b_kero_atf = 0
  const Index r_pool_atf = add_row("Pool_Jet", 0.0, 0.0);
  entries.push_back({r_pool_atf, col_products["ATF_Jet"], 1.0});
  entries.push_back({r_pool_atf, col_b_kero_atf, -1.0});

  const Index r_kero_avail = add_row("Kero_availability", -kInfinity, 0.0);
  entries.push_back({r_kero_avail, col_b_kero_atf, 1.0});
  entries.push_back({r_kero_avail, col_cdu_kero, -1.0});

  // 4. HSD Diesel Pool: y_HSD - (b_dhds_hsd + b_lgo_hsd) = 0
  const Index r_pool_hsd = add_row("Pool_Diesel", 0.0, 0.0);
  entries.push_back({r_pool_hsd, col_products["HSD_Diesel"], 1.0});
  entries.push_back({r_pool_hsd, col_b_dhds_hsd, -1.0});
  entries.push_back({r_pool_hsd, col_b_lgo_hsd, -1.0});

  const Index r_dhds_avail = add_row("DHDS_diesel_avail", -kInfinity, 0.0);
  entries.push_back({r_dhds_avail, col_b_dhds_hsd, 1.0});
  entries.push_back({r_dhds_avail, col_dhds_diesel, -1.0});

  // 5. Fuel Oil Pool: y_FO - (b_vr_fo + b_ar_fo + b_vgo_fo) = 0
  const Index r_pool_fo = add_row("Pool_Fuel_Oil", 0.0, 0.0);
  entries.push_back({r_pool_fo, col_products["Fuel_Oil"], 1.0});
  entries.push_back({r_pool_fo, col_b_vr_fo, -1.0});
  entries.push_back({r_pool_fo, col_b_ar_fo, -1.0});
  entries.push_back({r_pool_fo, col_b_vgo_fo, -1.0});

  const Index r_vr_avail = add_row("VR_availability", -kInfinity, 0.0);
  entries.push_back({r_vr_avail, col_b_vr_fo, 1.0});
  entries.push_back({r_vr_avail, col_vdu_vr, -1.0});

  // =======================================================================================
  // Quality Blending Constraints
  // =======================================================================================
  // 1. Gasoline Octane Requirement: (68 - target) * LN + (100 - target) * Ref + (92 - target) * FCC_Gas >= 0
  double target_ron = 91.0;
  for (const auto& p : config.products) {
    if (p.name == "MS_Gasoline" && p.min_octane_ron > 0.0) target_ron = p.min_octane_ron;
  }
  const Index r_qual_octane = add_row("Quality_MS_Octane", 0.0, kInfinity);
  entries.push_back({r_qual_octane, col_b_ln_ms, 68.0 - target_ron});
  entries.push_back({r_qual_octane, col_b_ref_ms, 100.0 - target_ron});
  entries.push_back({r_qual_octane, col_b_fccgas_ms, 92.0 - target_ron});

  // 2. Diesel Sulfur Limit: (8 - max_s) * b_dhds_hsd + (5000 - max_s) * b_lgo_hsd <= 0
  double target_diesel_s = 10.0;
  for (const auto& p : config.products) {
    if (p.name == "HSD_Diesel" && p.max_sulfur_ppm < 1e6) target_diesel_s = p.max_sulfur_ppm;
  }
  const Index r_qual_sulfur = add_row("Quality_Diesel_Sulfur", -kInfinity, 0.0);
  entries.push_back({r_qual_sulfur, col_b_dhds_hsd, 8.0 - target_diesel_s});
  entries.push_back({r_qual_sulfur, col_b_lgo_hsd, 5000.0 - target_diesel_s});

  // Discrete MILP Linking constraints if enabled
  if (config.enable_discrete_milp && col_z_fcc != -1 && col_z_ccr != -1) {
    // FCC: 20 * z_fcc <= u_fcc <= 65 * z_fcc
    const Index r_fcc_min = add_row("FCC_min_turndown", -kInfinity, 0.0);
    entries.push_back({r_fcc_min, col_z_fcc, 20.0});
    entries.push_back({r_fcc_min, col_u_fcc, -1.0});

    const Index r_fcc_max = add_row("FCC_max_activation", -kInfinity, 0.0);
    entries.push_back({r_fcc_max, col_u_fcc, 1.0});
    entries.push_back({r_fcc_max, col_z_fcc, -fcc_cap});
  }

  // Populate Model Arrays
  model.col_names = std::move(col_names);
  model.col_cost = std::move(col_cost);
  model.col_lower = std::move(col_lower);
  model.col_upper = std::move(col_upper);
  model.is_integer = std::move(is_integer);

  model.row_names = std::move(row_names);
  model.row_lower = std::move(row_lower);
  model.row_upper = std::move(row_upper);

  // Sparse CSR Matrix Assembly
  model.matrix.reset(model.num_rows(), model.num_cols());
  for (const auto& e : entries) {
    model.matrix.add_entry(e.row, e.col, e.val);
  }
  model.matrix.finalize();

  // Optional Convex QP Hessian Assembly: quadratic penalty on deviation from mid demand
  if (config.quadratic_penalty_weight > 0.0) {
    model.hessian.reset(model.num_cols(), model.num_cols());
    for (const auto& [p_name, c_idx] : col_products) {
      model.hessian.add_entry(c_idx, c_idx, config.quadratic_penalty_weight);
    }
    model.hessian.finalize();
  }

  return model;
}

RefineryResult RefineryTwin::run_scenario(const ScenarioConfig& config,
                                          const Options& options) const {
  RefineryResult result;
  result.config = config;

  // 1. Formulate optimization model
  const Model model = build_optimization_model(config);

  // 2. Adaptive Solver Backend Selection & Optimization
  result.solver_solution = solve(model, options);

  // 3. Independent Verification through Trust Layer
  result.verification_report = verify_solution(model, result.solver_solution, options);

  // 4. Extract Industrial KPIs if feasible/optimal point exists
  if (claims_a_point(result.solver_solution.status) &&
      !result.solver_solution.col_value.empty()) {
    RefineryKPIs& kpi = result.kpis;
    const auto& vals = result.solver_solution.col_value;

    double crude_proc = 0.0;
    double crude_cost = 0.0;
    for (std::size_t i = 0; i < config.crudes.size(); ++i) {
      const double x_k = vals[i];
      crude_proc += x_k;
      crude_cost += x_k * config.crudes[i].price_per_bbl;
      kpi.crude_allocation_kbpd[config.crudes[i].name] = x_k;
      kpi.crude_utilization_pct[config.crudes[i].name] =
          (config.crudes[i].max_avail_kbpd > 0.0) ? (x_k / config.crudes[i].max_avail_kbpd) * 100.0 : 0.0;
    }
    kpi.total_crude_processed_kbpd = crude_proc;
    kpi.total_crude_cost_k_usd = crude_cost;

    // Unit Throughputs
    for (Index j = 0; j < model.num_cols(); ++j) {
      const auto& name = model.col_names[static_cast<std::size_t>(j)];
      if (name.rfind("u_", 0) == 0) {
        const std::string unit_name = name.substr(2);
        const double flow = vals[static_cast<std::size_t>(j)];
        kpi.unit_throughput_kbpd[unit_name] = flow;
        for (const auto& u : config.units) {
          if (u.name == unit_name) {
            kpi.unit_utilization_pct[unit_name] = (u.capacity_kbpd > 0.0) ? (flow / u.capacity_kbpd) * 100.0 : 0.0;
            kpi.total_operating_cost_k_usd += flow * u.operating_cost_per_bbl;
            break;
          }
        }
      }
    }

    // Finished Product Production & Revenue
    double total_prod = 0.0;
    double gross_rev = 0.0;
    for (Index j = 0; j < model.num_cols(); ++j) {
      const auto& name = model.col_names[static_cast<std::size_t>(j)];
      if (name.rfind("y_", 0) == 0) {
        const std::string prod_name = name.substr(2);
        const double prod_vol = vals[static_cast<std::size_t>(j)];
        kpi.product_production_kbpd[prod_name] = prod_vol;
        total_prod += prod_vol;
        for (const auto& p : config.products) {
          if (p.name == prod_name) {
            gross_rev += prod_vol * p.selling_price_per_bbl;
            kpi.demand_fulfillment_pct[prod_name] =
                (p.max_demand_kbpd > 0.0) ? (prod_vol / p.max_demand_kbpd) * 100.0 : 0.0;
            break;
          }
        }
      }
    }
    kpi.total_products_produced_kbpd = total_prod;
    kpi.total_gross_revenue_k_usd = gross_rev;
    kpi.net_operating_margin_k_usd = gross_rev - crude_cost - kpi.total_operating_cost_k_usd;
    kpi.net_margin_per_bbl = (crude_proc > 0.0) ? (kpi.net_operating_margin_k_usd / crude_proc) : 0.0;

    // Detect Active Bottlenecks (Activity within 1e-5 of finite bounds)
    std::vector<double> activity(static_cast<std::size_t>(model.num_rows()), 0.0);
    model.matrix.multiply_add(vals.data(), activity.data(), 1.0);
    for (Index i = 0; i < model.num_rows(); ++i) {
      const auto u = static_cast<std::size_t>(i);
      const double act = activity[u];
      if (is_finite_bound(model.row_upper[u]) && std::fabs(act - model.row_upper[u]) <= 1e-5) {
        kpi.binding_bottlenecks.push_back(model.row_names[u] + " (Upper Bound)");
      } else if (is_finite_bound(model.row_lower[u]) && std::fabs(act - model.row_lower[u]) <= 1e-5) {
        kpi.binding_bottlenecks.push_back(model.row_names[u] + " (Lower Bound)");
      }
    }

    kpi.backend_used = result.verification_report.selected_backend;
    kpi.solve_seconds = result.solver_solution.solve_seconds;
    kpi.verification_status = result.verification_report.status;
    kpi.max_residual = result.verification_report.max_primal_violation;
    kpi.is_verified_trustworthy = result.verification_report.passed();
  }

  // Summary generation
  std::ostringstream ss;
  ss << "=== REFINERY DIGITAL TWIN OPTIMIZATION RESULT ===\n";
  ss << fmt::format("Scenario: {}\n", config.name);
  ss << fmt::format("Solver Status: {} | Trust Layer: {}\n",
                    to_string(result.solver_solution.status),
                    to_string(result.verification_report.status));
  if (claims_a_point(result.solver_solution.status)) {
    ss << fmt::format("Total Crude Processed: {:.1f} kbpd\n", result.kpis.total_crude_processed_kbpd);
    ss << fmt::format("Gross Revenue:        ${:.2f}k / day\n", result.kpis.total_gross_revenue_k_usd);
    ss << fmt::format("Crude Cost:           ${:.2f}k / day\n", result.kpis.total_crude_cost_k_usd);
    ss << fmt::format("Operating Cost:       ${:.2f}k / day\n", result.kpis.total_operating_cost_k_usd);
    ss << fmt::format("Net Operating Margin: ${:.2f}k / day (GRM: ${:.2f}/bbl)\n",
                      result.kpis.net_operating_margin_k_usd, result.kpis.net_margin_per_bbl);
    ss << "Refinery Unit Utilizations:\n";
    for (const auto& [u_name, util] : result.kpis.unit_utilization_pct) {
      ss << fmt::format("  {:<8}: {:5.1f}% ({:.1f} kbpd)\n",
                        u_name, util, result.kpis.unit_throughput_kbpd[u_name]);
    }
  } else {
    ss << fmt::format("Explanation: {}\n", result.solver_solution.message);
  }
  result.summary_text = ss.str();

  return result;
}

ScenarioComparison RefineryTwin::compare_scenarios(const RefineryResult& baseline,
                                                   const RefineryResult& scenario) {
  ScenarioComparison comp;
  comp.baseline_name = baseline.config.name;
  comp.scenario_name = scenario.config.name;

  if (!claims_a_point(baseline.solver_solution.status) ||
      !claims_a_point(scenario.solver_solution.status)) {
    comp.comparative_analysis_report = fmt::format(
        "Direct numerical comparison unavailable: Baseline status={}, Scenario status={}",
        to_string(baseline.solver_solution.status),
        to_string(scenario.solver_solution.status));
    return comp;
  }

  const auto& b = baseline.kpis;
  const auto& s = scenario.kpis;

  comp.net_margin_delta_k_usd = s.net_operating_margin_k_usd - b.net_operating_margin_k_usd;
  comp.net_margin_delta_pct = (b.net_operating_margin_k_usd != 0.0)
      ? (comp.net_margin_delta_k_usd / std::fabs(b.net_operating_margin_k_usd)) * 100.0 : 0.0;
  comp.crude_processed_delta_kbpd = s.total_crude_processed_kbpd - b.total_crude_processed_kbpd;
  comp.production_delta_kbpd = s.total_products_produced_kbpd - b.total_products_produced_kbpd;

  for (const auto& [prod, vol] : s.product_production_kbpd) {
    const double b_vol = b.product_production_kbpd.count(prod) ? b.product_production_kbpd.at(prod) : 0.0;
    comp.product_production_deltas[prod] = vol - b_vol;
  }

  for (const auto& [u, util] : s.unit_utilization_pct) {
    const double b_util = b.unit_utilization_pct.count(u) ? b.unit_utilization_pct.at(u) : 0.0;
    comp.unit_utilization_deltas[u] = util - b_util;
  }

  for (const auto& b_neck : s.binding_bottlenecks) {
    if (std::find(b.binding_bottlenecks.begin(), b.binding_bottlenecks.end(), b_neck) == b.binding_bottlenecks.end()) {
      comp.new_active_bottlenecks.push_back(b_neck);
    }
  }

  for (const auto& b_neck : b.binding_bottlenecks) {
    if (std::find(s.binding_bottlenecks.begin(), s.binding_bottlenecks.end(), b_neck) == s.binding_bottlenecks.end()) {
      comp.relieved_bottlenecks.push_back(b_neck);
    }
  }

  std::ostringstream ss;
  ss << "=== WHAT-IF DIFFERENTIAL ANALYSIS ===\n";
  ss << fmt::format("Baseline: {}\n", comp.baseline_name);
  ss << fmt::format("Scenario: {}\n", comp.scenario_name);
  ss << fmt::format("Net Margin Impact:   {:+8.2f}k USD/day ({:+5.1f}%)\n",
                    comp.net_margin_delta_k_usd, comp.net_margin_delta_pct);
  ss << fmt::format("Crude Distillation:  {:+8.1f} kbpd\n", comp.crude_processed_delta_kbpd);
  ss << fmt::format("Product Output:      {:+8.1f} kbpd\n", comp.production_delta_kbpd);
  ss << "Key Shift in Production Mix:\n";
  for (const auto& [prod, delta] : comp.product_production_deltas) {
    if (std::fabs(delta) > 0.01) {
      ss << fmt::format("  {:<12}: {:+6.1f} kbpd\n", prod, delta);
    }
  }
  if (!comp.new_active_bottlenecks.empty()) {
    ss << "Newly Emergent Bottlenecks:\n";
    for (const auto& b_name : comp.new_active_bottlenecks) {
      ss << "  [!] " << b_name << "\n";
    }
  }
  comp.comparative_analysis_report = ss.str();

  return comp;
}

}  // namespace sankhya::refinery
