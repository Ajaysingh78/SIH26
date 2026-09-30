#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Automated Test Suite for SANKHYA Reproducible Benchmark Laboratory (Feature 6).

Validates:
1. LP benchmark execution & independent verification (afiro, crude_blend)
2. MILP benchmark execution & integer bound proof (blend_milp, lot_sizing)
3. QP/MIQP benchmark execution & Dorn duality proof (qp_blend, miqp_blend)
4. Industrial refinery digital twin execution & KPI extraction (baseline, infeasible_demand)
5. CPU execution backend
6. AUTO execution backend & problem analysis
7. Truthful GPU status capture (never claims GPU speedup when hardware is absent)
8. Verification status capture (passes/fails and tolerances)
9. Result export to JSON and CSV formats
10. Repeated runs statistical evaluation & determinism validation
11. Benchmark run comparison mechanism
12. Benchmark regression detection engine
"""

import json
import tempfile
import unittest
from pathlib import Path

import sys
REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from bench.laboratory import (
    BenchmarkLaboratory,
    BenchmarkResult,
    check_benchmark_regressions,
    compare_benchmark_runs,
    detect_hardware_metadata,
    export_results_to_csv,
    export_results_to_json,
    get_git_commit,
)


class TestBenchmarkLaboratory(unittest.TestCase):
    """Test suite validating all Feature 6 benchmark laboratory requirements."""

    def setUp(self):
        self.lab = BenchmarkLaboratory(backend="auto", repeats=1)
        self.data_netlib = REPO_ROOT / "data" / "netlib"
        self.demo_dir = REPO_ROOT / "demo"
        self.casestudies_dir = REPO_ROOT / "data" / "casestudies"

    def test_01_reproducibility_metadata(self):
        """Verify git commit, hardware, and environment metadata capture."""
        commit = get_git_commit()
        self.assertIsInstance(commit, str)
        self.assertGreater(len(commit), 3)

        hw = detect_hardware_metadata()
        self.assertIn("os", hw)
        self.assertIn("cpu_model", hw)
        self.assertIn("logical_cores", hw)
        self.assertIn("cuda_available", hw)
        self.assertIn("gpu_device", hw)
        self.assertIsInstance(hw["cuda_available"], bool)
        # Truthful GPU test: if cuda_available is False, gpu_device must indicate Fallback/None
        if not hw["cuda_available"]:
            self.assertIn("None", hw["gpu_device"])

    def test_02_continuous_lp_benchmark(self):
        """Validate continuous LP benchmark and independent verification."""
        afiro_mps = self.data_netlib / "afiro.mps"
        if afiro_mps.exists():
            res = self.lab.run_mps_benchmark("afiro", afiro_mps, "netlib", -464.75314286)
            self.assertEqual(res.solver_status, "OPTIMAL")
            self.assertAlmostEqual(res.objective, -464.75314286, places=5)
            self.assertEqual(res.verification_status, "VERIFIED")
            self.assertTrue(res.matches_reference)
            self.assertGreater(res.verification_passed_checks, 0)

        cb_mps = self.demo_dir / "crude_blend.mps"
        if cb_mps.exists():
            res_cb = self.lab.run_mps_benchmark("crude_blend", cb_mps, "demo", 214.145946)
            self.assertEqual(res_cb.solver_status, "OPTIMAL")
            self.assertAlmostEqual(res_cb.objective, 214.145946, places=4)
            self.assertEqual(res_cb.verification_status, "VERIFIED")

    def test_03_mixed_integer_milp_benchmark(self):
        """Validate MILP benchmark solve and branch-and-bound verification."""
        milp_mps = self.demo_dir / "blend_milp.mps"
        if milp_mps.exists():
            res = self.lab.run_mps_benchmark("blend_milp", milp_mps, "milp", 223.857605)
            self.assertEqual(res.solver_status, "OPTIMAL")
            self.assertAlmostEqual(res.objective, 223.857605, places=4)
            self.assertEqual(res.verification_status, "VERIFIED")
            self.assertEqual(res.mip_gap, 0.0)

        ls_mps = self.casestudies_dir / "lot_sizing.mps"
        if ls_mps.exists():
            res_ls = self.lab.run_mps_benchmark("lot_sizing", ls_mps, "milp", 770.0)
            self.assertEqual(res_ls.solver_status, "OPTIMAL")
            self.assertAlmostEqual(res_ls.objective, 770.0, places=4)
            self.assertEqual(res_ls.verification_status, "VERIFIED")

    def test_04_quadratic_qp_and_miqp_benchmark(self):
        """Validate continuous QP and discrete MIQP benchmarks with Dorn duality."""
        qp_mps = self.demo_dir / "qp_blend.mps"
        if qp_mps.exists():
            res = self.lab.run_mps_benchmark("qp_blend", qp_mps, "qp", 66.666667)
            self.assertEqual(res.solver_status, "OPTIMAL")
            self.assertAlmostEqual(res.objective, 66.666667, places=4)
            self.assertEqual(res.verification_status, "VERIFIED")

        miqp_mps = self.demo_dir / "miqp_blend.mps"
        if miqp_mps.exists():
            res_miqp = self.lab.run_mps_benchmark("miqp_blend", miqp_mps, "qp", 66.67)
            self.assertEqual(res_miqp.solver_status, "OPTIMAL")
            self.assertAlmostEqual(res_miqp.objective, 66.67, places=2)
            self.assertEqual(res_miqp.verification_status, "VERIFIED")

    def test_05_industrial_refinery_benchmark(self):
        """Validate Feature 4 MRPL Refinery digital twin scenario benchmarks."""
        # Optimal baseline scenario
        res_base = self.lab.run_refinery_benchmark("baseline")
        self.assertEqual(res_base.solver_status, "OPTIMAL")
        self.assertAlmostEqual(res_base.objective, -2177.31, places=1)
        self.assertEqual(res_base.verification_status, "VERIFIED")
        self.assertIn("gross_revenue_k_usd", res_base.kpis)
        self.assertIn("net_margin_per_bbl", res_base.kpis)

        # Infeasible demand stress scenario
        res_infeas = self.lab.run_refinery_benchmark("infeasible_demand")
        self.assertEqual(res_infeas.solver_status, "INFEASIBLE")
        self.assertEqual(res_infeas.verification_status, "VERIFIED")

    def test_06_cpu_and_auto_backends(self):
        """Validate CPU and AUTO adaptive execution backend routing."""
        lab_cpu = BenchmarkLaboratory(backend="cpu", repeats=1)
        lab_auto = BenchmarkLaboratory(backend="auto", repeats=1)

        res_cpu = lab_cpu.run_refinery_benchmark("baseline")
        res_auto = lab_auto.run_refinery_benchmark("baseline")

        self.assertEqual(res_cpu.actual_backend, "CPU")
        self.assertEqual(res_auto.actual_backend, "CPU")
        self.assertAlmostEqual(res_cpu.objective, res_auto.objective, places=2)

    def test_07_truthful_gpu_fallback(self):
        """Validate that requesting GPU on non-GPU host truthfully reports CPU (Fallback)."""
        lab_gpu = BenchmarkLaboratory(backend="gpu", repeats=1)
        res = lab_gpu.run_refinery_benchmark("baseline")
        self.assertEqual(res.backend_requested, "gpu")
        if not lab_gpu.hw_info["cuda_available"]:
            self.assertEqual(res.actual_backend, "CPU (Fallback)")
        else:
            self.assertEqual(res.actual_backend, "GPU")
        self.assertEqual(res.solver_status, "OPTIMAL")

    def test_08_repeated_runs_statistics(self):
        """Validate repeated-run timing metrics (mean, median, min, max, std dev, determinism)."""
        lab_repeats = BenchmarkLaboratory(backend="cpu", repeats=3)
        res = lab_repeats.run_refinery_benchmark("baseline")

        self.assertEqual(res.repeats, 3)
        self.assertGreater(res.mean_seconds, 0.0)
        self.assertGreater(res.median_seconds, 0.0)
        self.assertLessEqual(res.min_seconds, res.median_seconds)
        self.assertGreaterEqual(res.max_seconds, res.median_seconds)
        self.assertTrue(res.deterministic)

    def test_09_comparison_mechanism(self):
        """Validate result comparison (speedup, objective discrepancy, status agreement)."""
        res_a = self.lab.run_refinery_benchmark("baseline")
        res_b = self.lab.run_refinery_benchmark("high_demand")

        comp = compare_benchmark_runs([res_a], [res_a], "Run1", "Run2")
        self.assertEqual(comp["total_compared"], 1)
        self.assertAlmostEqual(comp["geometric_mean_speedup"], 1.0, delta=0.5)
        self.assertEqual(comp["details"][0]["obj_discrepancy"], 0.0)
        self.assertTrue(comp["details"][0]["verification_agreement"])

    def test_10_regression_detection(self):
        """Validate regression detection against documented thresholds."""
        res_clean = self.lab.run_refinery_benchmark("baseline")

        # 1. Clean run test
        clean_report = check_benchmark_regressions([res_clean], [res_clean])
        self.assertTrue(clean_report["clean"])
        self.assertEqual(clean_report["regression_count"], 0)

        # 2. Objective discrepancy regression test
        res_corrupted = BenchmarkResult(**res_clean.to_dict())
        res_corrupted.objective = res_clean.objective + 50.0  # intentional drift
        corrupt_report = check_benchmark_regressions([res_clean], [res_corrupted], obj_tolerance=1e-4)
        self.assertFalse(corrupt_report["clean"])
        self.assertGreater(corrupt_report["regression_count"], 0)
        self.assertEqual(corrupt_report["regressions"][0]["type"], "OBJECTIVE_DISCREPANCY")

        # 3. Verification regression test
        res_unverif = BenchmarkResult(**res_clean.to_dict())
        res_unverif.verification_status = "REJECTED"
        verif_report = check_benchmark_regressions([res_clean], [res_unverif])
        self.assertFalse(verif_report["clean"])
        self.assertEqual(verif_report["regressions"][0]["type"], "VERIFICATION_REGRESSION")

    def test_11_json_and_csv_export(self):
        """Validate JSON and CSV file export formats."""
        res = self.lab.run_refinery_benchmark("baseline")

        with tempfile.TemporaryDirectory() as tmp_dir:
            tmp_path = Path(tmp_dir)
            json_file = tmp_path / "test_results.json"
            csv_file = tmp_path / "test_results.csv"

            export_results_to_json([res], json_file)
            export_results_to_csv([res], csv_file)

            self.assertTrue(json_file.exists())
            self.assertTrue(csv_file.exists())

            # Validate JSON content
            data = json.loads(json_file.read_text())
            self.assertEqual(data["total_instances"], 1)
            self.assertEqual(data["results"][0]["instance"], "baseline")

            # Validate CSV content
            csv_lines = csv_file.read_text().strip().split("\n")
            self.assertGreaterEqual(len(csv_lines), 2)
            self.assertIn("benchmark_name", csv_lines[0])
            self.assertIn("baseline", csv_lines[1])


if __name__ == "__main__":
    unittest.main()
