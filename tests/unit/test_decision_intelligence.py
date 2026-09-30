#!/usr/bin/env python3
"""
test_decision_intelligence.py — Comprehensive Unit Test Suite for Phase 2 Feature 7
(Industrial Decision Intelligence)

Validates the 12 core requirements:
1. Verified optimal result -> authoritative decision insights generated.
2. Unverified result -> authoritative insights blocked / labeled.
3. Capacity bottleneck detected correctly (CDU in baseline, FCC in unit_constraint).
4. Binding constraint detected correctly (e.g., desulfurization ceiling, quality specs).
5. Resource utilization calculated correctly (exhausted crude quotas, unit loadings).
6. Scenario comparison produces correct differences (objective, crude intake, unit loading).
7. Limited-crude scenario insight (crude starvation, -58.0 kbpd throughput).
8. High-demand scenario insight (CDU at 100% capacity ceiling, margin identical).
9. Unit-capacity scenario insight (FCC derating 50%, VGO diverted to fuel oil).
10. Quality-constraint scenario insight (95 RON + 8 ppm sulfur, LGO hydrotreated).
11. Infeasible scenario handled correctly (dual Farkas certificate, recommendations blocked).
12. Dashboard receives structured insight data (audit gate, executive summary, bottlenecks, yields, recommendations).
"""

import sys
import os
import unittest
import copy
from pathlib import Path

# Add web-ui directory to path
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
WEB_UI_DIR = REPO_ROOT / "web-ui"
sys.path.insert(0, str(WEB_UI_DIR))

from refinery_engine import RefineryDigitalTwinBackend
from decision_intelligence import IndustrialDecisionEngine, IndustrialDecisionPackage


class TestIndustrialDecisionIntelligence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Solve all standard refinery scenarios using the digital twin engine
        cls.baseline_res = RefineryDigitalTwinBackend.run("baseline", "cpu")
        cls.high_demand_res = RefineryDigitalTwinBackend.run("high_demand", "cpu")
        cls.limited_crude_res = RefineryDigitalTwinBackend.run("limited_crude", "cpu")
        cls.unit_constraint_res = RefineryDigitalTwinBackend.run("unit_constraint", "cpu")
        cls.quality_constraint_res = RefineryDigitalTwinBackend.run("quality_constraint", "cpu")
        cls.infeasible_res = RefineryDigitalTwinBackend.run("infeasible_demand", "cpu")

    def test_01_verified_optimal_result_insights_generated(self):
        """Test 1: Verified optimal result -> authoritative insights generated."""
        pkg = IndustrialDecisionEngine.synthesize_decision_package(self.baseline_res)
        
        self.assertIsInstance(pkg, IndustrialDecisionPackage)
        self.assertTrue(pkg.audit_gate.is_authoritative)
        self.assertEqual(pkg.audit_gate.status_badge, "VERIFIED OPTIMAL")
        self.assertIn("authoritative", pkg.audit_gate.gate_message.lower())
        
        # Verify executive summary presence and values
        self.assertIsNotNone(pkg.executive_summary)
        base_kpis = self.baseline_res["kpis"]
        self.assertAlmostEqual(
            pkg.executive_summary.net_operating_margin_k_usd,
            base_kpis["net_operating_margin_k_usd"],
            places=2
        )
        self.assertAlmostEqual(pkg.executive_summary.total_crude_processed_kbpd, 300.0, places=2)
        
        # Recommendations must be authorized and non-empty
        self.assertGreater(len(pkg.actionable_recommendations), 0)
        for rec in pkg.actionable_recommendations:
            self.assertIn(rec.priority, ["IMMEDIATE", "HIGH", "MEDIUM", "LOW"])
            self.assertTrue(len(rec.mathematical_basis) > 10)

    def test_02_unverified_result_authoritative_insights_blocked(self):
        """Test 2: Unverified result -> authoritative insights blocked and labeled."""
        tampered_res = copy.deepcopy(self.baseline_res)
        # Violate primal residual check
        tampered_res["verification"]["is_trustworthy"] = False
        tampered_res["verification"]["status"] = "AUDIT REJECTED"
        tampered_res["verification"]["max_primal_violation"] = 0.05  # > 1e-7 tolerance
        
        pkg = IndustrialDecisionEngine.synthesize_decision_package(tampered_res)
        
        self.assertFalse(pkg.audit_gate.is_authoritative)
        self.assertEqual(pkg.audit_gate.status_badge, "AUDIT REJECTED (NON-AUTHORITATIVE)")
        self.assertIn("withheld", pkg.audit_gate.gate_message.lower())
        
        # Operational recommendations must be blocked or hold-only
        rec_categories = [r.category for r in pkg.actionable_recommendations]
        self.assertTrue(all(c == "AUDIT_HOLD" for c in rec_categories))

    def test_03_capacity_bottleneck_detected_correctly(self):
        """Test 3: Capacity bottleneck detected correctly (CDU in baseline, FCC in unit_constraint)."""
        # Baseline: CDU is 300/300 kbpd (100.0%)
        pkg_base = IndustrialDecisionEngine.synthesize_decision_package(self.baseline_res)
        self.assertIn("CDU", pkg_base.resource_utilization.binding_units)
        
        cdu_diag = next(b for b in pkg_base.bottleneck_intelligence.critical_bottlenecks if "CDU" in b.resource)
        self.assertEqual(cdu_diag.severity, "CRITICAL")
        self.assertEqual(cdu_diag.category, "UNIT_CAPACITY")
        
        # Unit constraint: FCC is derated to 32.5 kbpd and maxed at 100.0%
        pkg_unit = IndustrialDecisionEngine.synthesize_decision_package(self.unit_constraint_res)
        self.assertIn("FCC", pkg_unit.resource_utilization.binding_units)
        fcc_diag = next(b for b in pkg_unit.bottleneck_intelligence.critical_bottlenecks if "FCC" in b.resource)
        self.assertEqual(fcc_diag.severity, "CRITICAL")

    def test_04_binding_constraint_detected_correctly(self):
        """Test 4: Binding constraint detected correctly."""
        # Quality constraint: Strict BS-VI sulfur spec binds (bypass is 0)
        pkg_qc = IndustrialDecisionEngine.synthesize_decision_package(self.quality_constraint_res)
        
        active_cats = [b.category for b in pkg_qc.bottleneck_intelligence.critical_bottlenecks]
        self.assertIn("QUALITY_SPEC", active_cats)

    def test_05_resource_utilization_calculated_correctly(self):
        """Test 5: Resource utilization calculated correctly."""
        pkg = IndustrialDecisionEngine.synthesize_decision_package(self.baseline_res)
        ru = pkg.resource_utilization
        
        # Check crudes
        self.assertEqual(len(ru.crudes), 6)
        crude_names = [c.crude_name for c in ru.crudes]
        self.assertIn("Arab_Light", crude_names)
        self.assertIn("Arab_Heavy", crude_names)
        
        # In baseline, Arab Light, Arab Heavy, Bonny Light, Murban, Maya are maxed (100%)
        self.assertIn("Arab_Heavy", ru.exhausted_crudes)
        self.assertIn("Arab_Light", ru.exhausted_crudes)
        
        # Check units
        self.assertEqual(len(ru.units), 5)
        cdu_item = next(u for u in ru.units if u.unit_name == "CDU")
        self.assertAlmostEqual(cdu_item.utilization_pct, 100.0, places=1)
        self.assertTrue(cdu_item.is_binding)

    def test_06_scenario_comparison_produces_correct_differences(self):
        """Test 6: Scenario comparison produces correct mathematical differences."""
        diff = IndustrialDecisionEngine.evaluate_scenario_differential(
            self.baseline_res, self.unit_constraint_res
        )
        
        # Margin delta in $k/day
        expected_margin_diff = (
            self.unit_constraint_res["kpis"]["net_operating_margin_k_usd"] -
            self.baseline_res["kpis"]["net_operating_margin_k_usd"]
        )
        self.assertAlmostEqual(diff.margin_delta_k_usd, expected_margin_diff, places=2)
        self.assertLess(diff.margin_delta_k_usd, 0.0)  # Derating causes profit loss
        
        # FCC throughput delta: 32.5 - 51.45 = -18.95 kbpd
        fcc_diff = diff.unit_throughput_deltas_kbpd["FCC"]
        self.assertAlmostEqual(fcc_diff, -18.95, places=2)
        
        # Primary reason must cite FCC turnaround / derating
        self.assertIn("FCC", diff.root_cause_explanation)

    def test_07_limited_crude_scenario_insight(self):
        """Test 7: Limited-crude scenario insight."""
        pkg = IndustrialDecisionEngine.synthesize_decision_package(
            self.limited_crude_res, self.baseline_res
        )
        diff = pkg.scenario_differential
        self.assertIsNotNone(diff)
        
        # In limited crude, sweet crude cuts drop total intake by 58.0 kbpd (from 300 to 242 kbpd)
        self.assertAlmostEqual(diff.crude_delta_kbpd, -58.0, places=1)
        self.assertAlmostEqual(pkg.executive_summary.total_crude_processed_kbpd, 242.0, places=1)
        
        # Primary driver must detect feedstock limitation
        self.assertIn("crude", diff.root_cause_explanation.lower())

    def test_08_high_demand_scenario_insight(self):
        """Test 8: High-demand scenario insight."""
        pkg = IndustrialDecisionEngine.synthesize_decision_package(
            self.high_demand_res, self.baseline_res
        )
        diff = pkg.scenario_differential
        self.assertIsNotNone(diff)
        
        # Baseline already maxed CDU at 300 kbpd; high demand is feasible with identical optimum
        self.assertAlmostEqual(diff.crude_delta_kbpd, 0.0, places=2)
        self.assertAlmostEqual(diff.margin_delta_k_usd, 0.0, places=2)
        
        # CDU remains the binding limit
        self.assertIn("CDU", pkg.resource_utilization.binding_units)

    def test_09_unit_capacity_scenario_insight(self):
        """Test 9: Unit-capacity scenario insight."""
        pkg = IndustrialDecisionEngine.synthesize_decision_package(
            self.unit_constraint_res, self.baseline_res
        )
        diff = pkg.scenario_differential
        self.assertIsNotNone(diff)
        
        # Margin penalty of -$463.13k/day
        self.assertAlmostEqual(diff.margin_delta_k_usd, -463.13, delta=2.0)
        
        # FCC utilization must be 100% of reduced 32.5 kbpd capacity
        fcc_item = next(u for u in pkg.resource_utilization.units if u.unit_name == "FCC")
        self.assertAlmostEqual(fcc_item.utilization_pct, 100.0, places=1)
        
        # VGO diverted into Fuel Oil (+10.7 kbpd)
        fo_delta = diff.product_deltas_kbpd["Fuel_Oil"]
        self.assertAlmostEqual(fo_delta, 10.7, places=1)

    def test_10_quality_constraint_scenario_insight(self):
        """Test 10: Quality-constraint scenario insight."""
        pkg = IndustrialDecisionEngine.synthesize_decision_package(
            self.quality_constraint_res, self.baseline_res
        )
        diff = pkg.scenario_differential
        self.assertIsNotNone(diff)
        
        # Operating margin drops by -$1,109.95k/day due to octane giveaway and full hydrotreating
        self.assertLess(diff.margin_delta_k_usd, -1000.0)
        
        # Quality constraint bottleneck must be detected
        active_cats = [b.category for b in pkg.bottleneck_intelligence.critical_bottlenecks]
        self.assertIn("QUALITY_SPEC", active_cats)

    def test_11_infeasible_scenario_handled_correctly(self):
        """Test 11: Infeasible scenario handled correctly."""
        pkg = IndustrialDecisionEngine.synthesize_decision_package(self.infeasible_res)
        
        # Must recognize infeasibility via Farkas ray
        self.assertEqual(pkg.audit_gate.status_badge, "VERIFIED INFEASIBLE")
        self.assertEqual(pkg.executive_summary.net_operating_margin_k_usd, 0.0)
        self.assertEqual(pkg.executive_summary.total_crude_processed_kbpd, 0.0)
        
        # Actionable recommendation must advise commercial renegotiation/force majeure
        self.assertEqual(len(pkg.actionable_recommendations), 1)
        rec = pkg.actionable_recommendations[0]
        self.assertEqual(rec.priority, "IMMEDIATE")
        self.assertEqual(rec.category, "COMMERCIAL_RENEGOTIATION")
        self.assertIn("Renegotiate", rec.action)
        self.assertIn("Farkas", rec.mathematical_basis)

    def test_12_dashboard_receives_structured_insight_data(self):
        """Test 12: Dashboard receives structured insight data."""
        # Convert package to JSON-compatible dict and check completeness
        pkg = IndustrialDecisionEngine.synthesize_decision_package(
            self.baseline_res, self.limited_crude_res
        )
        d = pkg.to_dict()
        
        # Validate primary keys required by dashboard
        expected_keys = [
            "scenario_key", "audit_gate", "executive_summary",
            "production_intelligence", "resource_utilization",
            "bottleneck_intelligence", "scenario_differential",
            "actionable_recommendations", "performance_evidence"
        ]
        for k in expected_keys:
            self.assertIn(k, d)
            self.assertIsNotNone(d[k])
            
        # Validate audit_gate schema
        self.assertIn("status_badge", d["audit_gate"])
        self.assertIn("is_authoritative", d["audit_gate"])
        self.assertIn("kkt_residual", d["audit_gate"])
        
        # Validate executive_summary schema
        self.assertIn("net_operating_margin_k_usd", d["executive_summary"])
        self.assertIn("total_crude_processed_kbpd", d["executive_summary"])
        self.assertIn("headline", d["executive_summary"])
        
        # Validate products in production_intelligence
        self.assertIsInstance(d["production_intelligence"]["products"], list)
        self.assertEqual(len(d["production_intelligence"]["products"]), 5)  # LPG, MS, ATF, HSD, FO


if __name__ == "__main__":
    unittest.main()
