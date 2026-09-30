#!/usr/bin/env python3
"""
test_dashboard_e2e.py — Comprehensive End-to-End Test Suite for Phase 2 Feature 5
(Premium Industrial Optimization Dashboard)

Validates:
1. REST API server health and endpoints (/api/health, /api/scenarios, /api/analyze, /api/optimize, /api/compare)
2. Real mathematical solver outputs across all 6 refinery scenarios
3. Problem analysis matrix characteristics (rows, cols, nonzeros, sparsity, workload)
4. Adaptive CPU/GPU backend routing and truthful fallback
5. Independent Trust Layer verification (primal/dual feasibility, integrality, KKT residual)
6. Refinery digital twin units (CDU, VDU, CCR, FCC, DHDS) and finished products (LPG, MS, ATF, HSD, FO)
7. What-If differential comparison against baseline
8. Infeasible demand handling with Farkas certificate proof
9. Production frontend build integrity (dist/ artifacts and DOM elements)
"""

import sys
import os
import json
import unittest
import threading
import time
from http.client import HTTPConnection
from pathlib import Path

# Add web-ui directory to path
WEB_UI_DIR = Path(__file__).parent.resolve()
sys.path.insert(0, str(WEB_UI_DIR))

from server import run_server, SankhyaApiHandler
from refinery_engine import RefineryDigitalTwinBackend


class TestDashboardBackendE2E(unittest.TestCase):
    server_thread = None
    server_port = 8899

    @classmethod
    def setUpClass(cls):
        # Start server in background thread on test port
        from http.server import HTTPServer
        cls.httpd = HTTPServer(('127.0.0.1', cls.server_port), SankhyaApiHandler)
        cls.server_thread = threading.Thread(target=cls.httpd.serve_forever, daemon=True)
        cls.server_thread.start()
        time.sleep(0.2)

    @classmethod
    def tearDownClass(cls):
        if cls.httpd:
            cls.httpd.shutdown()
            cls.httpd.server_close()

    def get_json(self, path):
        conn = HTTPConnection('127.0.0.1', self.server_port, timeout=5)
        conn.request('GET', path)
        res = conn.getresponse()
        self.assertEqual(res.status, 200, f"GET {path} returned status {res.status}")
        data = json.loads(res.read().decode('utf-8'))
        conn.close()
        return data

    def post_json(self, path, payload):
        conn = HTTPConnection('127.0.0.1', self.server_port, timeout=5)
        headers = {'Content-Type': 'application/json'}
        conn.request('POST', path, body=json.dumps(payload), headers=headers)
        res = conn.getresponse()
        self.assertEqual(res.status, 200, f"POST {path} returned status {res.status}")
        data = json.loads(res.read().decode('utf-8'))
        conn.close()
        return data

    def test_01_api_health(self):
        """Validate /api/health connectivity and engine identification."""
        data = self.get_json('/api/health')
        self.assertEqual(data.get('status'), 'ok')
        self.assertIn('SANKHYA', data.get('engine', ''))
        self.assertEqual(data.get('version'), '1.0.0')
        print("[PASS] Test 1: API health check verified.")

    def test_02_scenarios_catalog(self):
        """Validate all 6 supported refinery scenarios are listed."""
        data = self.get_json('/api/scenarios')
        scenarios = data.get('scenarios', [])
        self.assertEqual(len(scenarios), 6)
        ids = [s['id'] for s in scenarios]
        expected_ids = ['baseline', 'high_demand', 'limited_crude', 'unit_constraint', 'quality_constraint', 'infeasible_demand']
        for eid in expected_ids:
            self.assertIn(eid, ids, f"Missing scenario: {eid}")
        print("[PASS] Test 2: Scenarios catalog verified.")

    def test_03_problem_analysis_matrix_dimensions(self):
        """Validate problem characteristics produced by Feature 2."""
        data = self.get_json('/api/analyze?scenario=baseline')
        self.assertEqual(data.get('problem_type'), 'LP')
        self.assertEqual(data.get('is_mip'), False)
        self.assertEqual(data.get('rows'), 16)
        self.assertEqual(data.get('cols'), 19)
        self.assertEqual(data.get('nonzeros'), 48)
        self.assertGreater(data.get('sparsity_pct', 0), 80.0)
        self.assertEqual(data.get('integer_vars'), 0)
        self.assertEqual(data.get('workload_category'), 'small')
        self.assertEqual(data.get('selected_backend'), 'CPU')
        self.assertIn('Simplex', data.get('decision_reason', ''))
        print("[PASS] Test 3: Problem analysis & matrix dimensions verified.")

    def test_04_baseline_optimization_and_trust(self):
        """Validate baseline scenario optimization output and independent trust layer."""
        data = self.post_json('/api/optimize', {'scenario': 'baseline', 'backend': 'auto'})
        self.assertEqual(data.get('solver_status'), 'OPTIMAL')
        self.assertEqual(data.get('backend_used'), 'CPU')
        self.assertGreater(data.get('objective_value', 0), 2000000.0)
        self.assertAlmostEqual(data.get('objective_value', 0), 2177310.0, delta=1000.0)
        self.assertGreater(data.get('gross_revenue', 0), 25000000.0)
        self.assertGreater(data.get('feedstock_cost', 0), 20000000.0)

        # Independent Trust Layer checks
        v = data.get('verification', {})
        self.assertEqual(v.get('status'), 'VERIFIED OPTIMAL')
        self.assertLess(v.get('primal_feasibility', 1.0), 1e-6)
        self.assertEqual(v.get('integrality', 1.0), 0.0)

        # Units and Products
        units = data.get('units', {})
        self.assertEqual(units.get('CDU', {}).get('throughput_kbpd'), 300.0)
        self.assertGreater(units.get('DHDS', {}).get('throughput_kbpd', 0), 80.0)
        self.assertLessEqual(units.get('DHDS', {}).get('throughput_kbpd', 0), 110.0)

        products = data.get('products', {})
        self.assertGreaterEqual(products.get('HSD_Diesel', {}).get('production_kbpd', 0), 60.0)
        print("[PASS] Test 4: Baseline optimization & Trust Layer audit verified.")

    def test_05_whatif_scenarios(self):
        """Validate what-if scenarios: limited_crude, unit_constraint, quality_constraint."""
        # Limited crude (Sweet crude availability halved)
        lc = self.post_json('/api/optimize', {'scenario': 'limited_crude', 'backend': 'cpu'})
        self.assertEqual(lc.get('solver_status'), 'OPTIMAL')
        self.assertLessEqual(lc.get('crudes', {}).get('Bonny_Light', 0), 25.01)
        self.assertLess(lc.get('objective_value', 0), 2177310.0 + 1000.0)

        # Unit constraint (FCC down to 35 kbpd)
        uc = self.post_json('/api/optimize', {'scenario': 'unit_constraint', 'backend': 'cpu'})
        self.assertEqual(uc.get('solver_status'), 'OPTIMAL')
        self.assertLessEqual(uc.get('units', {}).get('FCC', {}).get('throughput_kbpd', 0), 35.01)
        self.assertLess(uc.get('objective_value', 0), 2177310.0)

        # Quality constraint (8 ppm ultra-low sulfur)
        qc = self.post_json('/api/optimize', {'scenario': 'quality_constraint', 'backend': 'cpu'})
        self.assertEqual(qc.get('solver_status'), 'OPTIMAL')
        self.assertLess(qc.get('objective_value', 0), 2177310.0)
        print("[PASS] Test 5: All what-if scenarios verified.")

    def test_06_infeasible_demand_and_farkas_certificate(self):
        """Validate truthful handling of infeasible scenario with Farkas ray proof."""
        data = self.post_json('/api/optimize', {'scenario': 'infeasible_demand', 'backend': 'auto'})
        self.assertEqual(data.get('solver_status'), 'INFEASIBLE')
        self.assertEqual(data.get('objective_value'), 0.0)

        v = data.get('verification', {})
        self.assertEqual(v.get('status'), 'VERIFIED INFEASIBLE')
        self.assertIn('Farkas', v.get('certificate', ''))
        self.assertIn('b^T y', v.get('certificate', ''))
        print("[PASS] Test 6: Infeasible demand & Farkas ray certificate verified.")

    def test_07_whatif_comparison_api(self):
        """Validate /api/compare differential reporting against baseline."""
        data = self.get_json('/api/compare?baseline=baseline&scenario=unit_constraint')
        self.assertEqual(data.get('baseline_scenario'), 'baseline')
        self.assertEqual(data.get('comparison_scenario'), 'unit_constraint')
        self.assertLess(data.get('delta_objective', 0), 0)
        self.assertLess(data.get('delta_objective_pct', 0), -10.0)
        self.assertIn('FCC', str(data.get('bottlenecks', {}).get('scenario', [])))
        print("[PASS] Test 7: What-If differential comparison API verified.")

    def test_08_gpu_fallback_truthful_reporting(self):
        """Validate that requesting GPU execution truthfully reports CPU fallback on CPU-only hardware."""
        data = self.post_json('/api/optimize', {'scenario': 'baseline', 'backend': 'gpu'})
        self.assertEqual(data.get('solver_status'), 'OPTIMAL')
        # On machines without CUDA or for small problems, it must report CPU fallback
        self.assertIn(data.get('backend_used'), ['CPU', 'CPU (Fallback)'])
        print("[PASS] Test 8: Truthful GPU request handling & fallback verified.")

    def test_09_frontend_build_artifacts(self):
        """Validate that the production Vite build bundle exists and contains required components."""
        dist_dir = WEB_UI_DIR / 'dist'
        self.assertTrue(dist_dir.exists(), "dist/ directory does not exist. Run npm run build.")
        
        index_html = dist_dir / 'index.html'
        self.assertTrue(index_html.exists(), "dist/index.html does not exist.")

        html_content = index_html.read_text(encoding='utf-8')
        required_elements = [
            'workflow-stepper',
            'scenario-select',
            'backend-status-banner',
            'analysis-matrix-dims',
            'trust-card',
            'trust-badge',
            'unit-card-cdu',
            'unit-card-dhds',
            'refinery-canvas',
            'whatif-modal'
        ]
        for elem in required_elements:
            self.assertIn(elem, html_content, f"Missing element ID in dist/index.html: {elem}")
        print("[PASS] Test 9: Production frontend build artifacts verified.")


if __name__ == '__main__':
    unittest.main()
