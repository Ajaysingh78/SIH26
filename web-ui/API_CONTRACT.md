# SANKHYA Industrial Optimization Dashboard — API Contract (v1.0)

This document formalizes the frontend-backend data contract for the SANKHYA Industrial Optimization Dashboard (Phase 2 Feature 5).

---

## 1. Overview & Architectural Principles

1. **Sovereign Source of Truth**: The SANKHYA optimization solver and refinery digital twin are the sole source of numerical truth. The frontend never fabricates solver metrics, primal/dual residuals, or shadow prices.
2. **Deterministic Schemas**: All responses adhere to strictly typed JSON structures.
3. **Independent Trust Layer**: Every optimization response includes independent verification fields certifying primal feasibility, dual feasibility, integrality, and KKT residuals.
4. **Truthful Hardware Reporting**: If GPU execution is requested but hardware or problem size does not warrant GPU offloading, the system reports `CPU (Fallback)` with explicit justification.

---

## 2. API Endpoints

### 2.1 Health & Engine Status

- **Route**: `GET /api/health`
- **Purpose**: Verify backend connectivity and solver engine availability.
- **Response**: `200 OK`
```json
{
  "status": "ok",
  "engine": "SANKHYA Sovereign C++ / Python Engine",
  "version": "1.0.0",
  "features": ["Feature 1 (GPU)", "Feature 2 (Adaptive)", "Feature 3 (Trust)", "Feature 4 (Twin)", "Feature 5 (Dashboard)"],
  "host": "localhost:8000"
}
```

---

### 2.2 Scenarios Catalog

- **Route**: `GET /api/scenarios`
- **Purpose**: Retrieve supported refinery what-if optimization scenarios.
- **Response**: `200 OK`
```json
{
  "scenarios": [
    {
      "id": "baseline",
      "name": "Baseline (Normal Operations)",
      "description": "300 kbpd CDU capacity, BS-VI standards, normal market demands"
    },
    {
      "id": "high_demand",
      "name": "High Product Demand",
      "description": "Increased diesel commitment (+20%) and gasoline commitments"
    },
    {
      "id": "limited_crude",
      "name": "Limited Sweet Crude Intake",
      "description": "Bonny Light supply capped at 25 kbpd (disruption simulation)"
    },
    {
      "id": "unit_constraint",
      "name": "Unit Bottleneck / Maintenance",
      "description": "FCC cracker capacity reduced from 65 kbpd to 35 kbpd"
    },
    {
      "id": "quality_constraint",
      "name": "Strict BS-VI Environmental Spec",
      "description": "Diesel pool max sulfur capped at 8 ppm (down from 10 ppm)"
    },
    {
      "id": "infeasible_demand",
      "name": "Infeasible Demand Stress Test",
      "description": "Diesel pool demand exceeds maximum achievable distillation yield"
    }
  ]
}
```

---

### 2.3 Problem Analysis & Hardware Routing

- **Route**: `GET /api/analyze?scenario={scenario_id}`
- **Purpose**: Analyze matrix structure and determine CPU/GPU/HYBRID routing prior to solve.
- **Response**: `200 OK`
```json
{
  "scenario": "baseline",
  "problem_type": "LP",
  "is_mip": false,
  "rows": 16,
  "cols": 19,
  "nonzeros": 48,
  "sparsity_pct": 84.21,
  "integer_vars": 0,
  "hardware": {
    "host_cpu": "x86_64 Host",
    "gpu_available": false,
    "gpu_device_count": 0
  },
  "workload_category": "small",
  "selected_backend": "CPU",
  "decision_reason": "Small formulation (19 vars < 2000 threshold); CPU Revised Simplex selected for zero overhead and direct convergence"
}
```

---

### 2.4 Optimization Execution

- **Route**: `POST /api/optimize`
- **Request Body**:
```json
{
  "scenario": "baseline",
  "backend": "auto"
}
```
*(Valid backends: `auto`, `cpu`, `gpu`)*

- **Response (Optimal Case)**: `200 OK`
```json
{
  "scenario": "baseline",
  "solver_status": "OPTIMAL",
  "backend_used": "CPU",
  "objective_value": 2177419.35,
  "gross_revenue": 26985000.0,
  "feedstock_cost": 24350000.0,
  "operating_cost": 457580.65,
  "solve_time_ms": 1.85,
  "simplex_iterations": 4,
  "problem_analysis": {
    "problem_type": "LP",
    "rows": 16,
    "cols": 19,
    "nonzeros": 48,
    "sparsity_pct": 84.21,
    "selected_backend": "CPU",
    "decision_reason": "..."
  },
  "verification": {
    "status": "VERIFIED OPTIMAL",
    "primal_feasibility": 2.84e-14,
    "dual_feasibility": 0.0,
    "integrality": 0.0,
    "kkt_residual": 2.84e-14,
    "tolerance": 1.0e-07,
    "certificate": "Primal & Dual Feasible (Zero Duality Gap)"
  },
  "crudes": {
    "Arab_Light": 50.0,
    "Arab_Heavy": 60.0,
    "Bonny_Light": 50.0,
    "Basrah_Medium": 40.0,
    "Murban": 60.0,
    "Maya": 40.0
  },
  "units": {
    "CDU": { "throughput_kbpd": 300.0, "capacity_kbpd": 300.0, "utilization_pct": 100.0, "is_binding": true },
    "VDU": { "throughput_kbpd": 140.0, "capacity_kbpd": 140.0, "utilization_pct": 100.0, "is_binding": true },
    "CCR": { "throughput_kbpd": 42.5, "capacity_kbpd": 45.0, "utilization_pct": 94.4, "is_binding": false },
    "FCC": { "throughput_kbpd": 58.2, "capacity_kbpd": 65.0, "utilization_pct": 89.5, "is_binding": false },
    "DHDS": { "throughput_kbpd": 110.0, "capacity_kbpd": 110.0, "utilization_pct": 100.0, "is_binding": true }
  },
  "products": {
    "LPG": { "production_kbpd": 12.5, "min_demand_kbpd": 10.0, "price_per_bbl": 72.0 },
    "MS_Gasoline": { "production_kbpd": 62.0, "min_demand_kbpd": 50.0, "price_per_bbl": 108.0 },
    "ATF_Jet": { "production_kbpd": 38.0, "min_demand_kbpd": 30.0, "price_per_bbl": 112.0 },
    "HSD_Diesel": { "production_kbpd": 110.0, "min_demand_kbpd": 90.0, "price_per_bbl": 105.0 },
    "Fuel_Oil": { "production_kbpd": 45.0, "min_demand_kbpd": 0.0, "price_per_bbl": 55.0 }
  },
  "active_bottlenecks": ["CDU Atmospheric Distillation", "DHDS Desulfurization"]
}
```

- **Response (Infeasible Case)**: `200 OK`
```json
{
  "scenario": "infeasible_demand",
  "solver_status": "INFEASIBLE",
  "backend_used": "CPU",
  "objective_value": 0.0,
  "verification": {
    "status": "VERIFIED INFEASIBLE",
    "certificate": "Farkas Infeasibility Ray: b^T y = -1.25e+02 < 0"
  },
  "active_bottlenecks": ["HSD Diesel Demand Exceeds Distillation Yield Capacity"]
}
```

---

### 2.5 What-If Differential Comparison

- **Route**: `GET /api/compare?baseline={baseline_id}&scenario={scenario_id}`
- **Purpose**: Calculate mathematical differences between baseline operations and scenario.
- **Response**: `200 OK`
```json
{
  "baseline_scenario": "baseline",
  "comparison_scenario": "unit_constraint",
  "delta_objective": -463612.90,
  "delta_objective_pct": -21.29,
  "bottlenecks": {
    "baseline": ["CDU", "DHDS"],
    "scenario": ["CDU", "FCC (Maintenance Cap 35 kbpd)", "DHDS"]
  },
  "crude_shifts_kbpd": { ... },
  "unit_throughput_shifts_kbpd": { ... },
  "product_yield_shifts_kbpd": { ... }
}
```
