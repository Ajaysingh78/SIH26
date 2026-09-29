# SANKHYA Industrial Digital Twin: MRPL-Style Refinery Optimization

## Public-Domain Synthetic Data & Benchmark Disclaimer

> **IMPORTANT DISCLAIMER:**
> This module is an **optimization-oriented synthetic refinery digital twin** designed exclusively for solver benchmarking, decision intelligence demonstration, and algorithmic evaluation.
>
> All crude assays, distillation yields, conversion unit parameters, stream blend indices, and operating costs are derived from public-domain chemical engineering textbooks (Gary & Handwerk, *Petroleum Refining: Technology and Economics*; Meyers, *Handbook of Petroleum Refining Processes*) and synthetic demo configurations.
>
> **This module does NOT contain, use, or represent confidential, proprietary, or operational data of Mangalore Refinery and Petrochemicals Limited (MRPL) or any other commercial refining entity.**

---

## 1. Executive Purpose

Modern industrial refineries represent high-dimensional, tightly coupled continuous and mixed-integer decision spaces. Operating margins (Gross Refining Margin, or GRM) depend on optimizing:
1. **Crude Basket Selection:** Allocating delivered crudes across high-sulfur (heavy/sour) and low-sulfur (light/sweet) grades subject to market availability and tanker delivery schedules.
2. **Atmospheric and Vacuum Distillation:** Separating crude into boiling-range cuts (LPG, Light Naphtha, Heavy Naphtha, Kerosene, Gas Oil, Residue).
3. **Secondary Upgrading and Conversion:** Loading downstream units—such as Catalytic Reforming (CCR), Fluidized Catalytic Cracking (FCC), and Diesel Hydrodesulfurization (DHDS)—to maximize high-value transport fuels.
4. **Finished Product Blending:** Meeting Bharat Stage VI (BS-VI) / Euro-VI specifications (e.g., $\le 10$ ppm sulfur for diesel and gasoline, $\ge 91$ or $95$ RON for motor spirit).

The **SANKHYA Refinery Digital Twin** encapsulates these planning and operational decisions directly within SANKHYA's native optimization framework, providing an end-to-end demonstration:

```
INDUSTRIAL SCENARIO
       ↓
MATHEMATICAL MODEL (LP / MILP / QP)
       ↓
ADAPTIVE BACKEND (CPU / GPU / HYBRID)
       ↓
SANKHYA SOLVER
       ↓
TRUST LAYER INDEPENDENT VERIFICATION
       ↓
INDUSTRIAL KPIS & WHAT-IF INSIGHT
```

---

## 2. Mathematical Formulation

### Objective Function
The digital twin maximizes daily Net Operating Margin (or equivalently, minimizes Net Cost):

$$\min \quad \sum_{k \in \mathcal{K}} \text{Price}_k \cdot x_k + \sum_{u \in \mathcal{U}} \text{OpCost}_u \cdot u_u - \sum_{p \in \mathcal{P}} \text{Price}_p \cdot y_p + \frac{1}{2} \sum_{p \in \mathcal{P}} \lambda_p (y_p - \bar{y}_p)^2$$

Where:
- $x_k \ge 0$: Crude purchase rate for crude $k \in \mathcal{K}$ (thousand barrels per day, kbpd)
- $u_u \ge 0$: Throughput across refinery unit $u \in \mathcal{U}$ (kbpd)
- $y_p \ge 0$: Sales rate of finished product $p \in \mathcal{P}$ (kbpd)
- $\lambda_p \ge 0$: Optional quadratic penalty parameter on demand swing volatility (producing a convex QP)

### Primary Decision Variables
| Variable Symbol | Representation | Domain |
|---|---|---|
| $x_k$ | Crude procurement for grade $k$ | $[0, \text{Avail}_k]$ kbpd |
| $u_{\text{CDU}}$ | Crude Distillation Unit throughput | $[0, \text{Cap}_{\text{CDU}}]$ kbpd |
| $u_{\text{VDU}}$ | Vacuum Distillation Unit throughput | $[0, \text{Cap}_{\text{VDU}}]$ kbpd |
| $u_{\text{CCR}}$ | Continuous Catalytic Reformer feed | $[0, \text{Cap}_{\text{CCR}}]$ kbpd |
| $u_{\text{FCC}}$ | Fluidized Catalytic Cracking feed | $[0, \text{Cap}_{\text{FCC}}]$ kbpd |
| $u_{\text{DHDS}}$ | Diesel Hydrodesulfurization feed | $[0, \text{Cap}_{\text{DHDS}}]$ kbpd |
| $v_{s, p}$ | Intermediate stream $s$ routed to product pool $p$ | $[0, \infty)$ kbpd |
| $y_p$ | Finished product production and sales | $[\text{Demand}_p^{\min}, \text{Demand}_p^{\max}]$ kbpd |
| $z_u$ | Discrete unit activation switch (MILP mode) | $\{0, 1\}$ |

### Core Constraints
1. **CDU Throughput & Balance:**
   $$u_{\text{CDU}} - \sum_{k \in \mathcal{K}} x_k = 0, \quad u_{\text{CDU}} \le \text{Cap}_{\text{CDU}}$$
2. **Crude Cut Separation:**
   $$\text{Cut}_s = \sum_{k \in \mathcal{K}} Y_{s, k} \cdot x_k \quad \forall s \in \{\text{LPG}, \text{LN}, \text{HN}, \text{KERO}, \text{LGO}, \text{AR}\}$$
3. **Secondary Unit Balances:**
   - CCR Feed: $u_{\text{CCR}} \le \text{Cut}_{\text{HN}}$, producing Reformate ($0.88 u_{\text{CCR}}$) and LPG ($0.08 u_{\text{CCR}}$).
   - VDU Feed: $u_{\text{VDU}} + b_{\text{AR\_FO}} = \text{Cut}_{\text{AR}}$, yielding Vacuum Gas Oil ($0.55 u_{\text{VDU}}$) and Vacuum Residue ($0.45 u_{\text{VDU}}$).
   - FCC Feed: $u_{\text{FCC}} \le \text{VGO}$, yielding FCC Gasoline ($0.52 u_{\text{FCC}}$), Light Cycle Oil ($0.24 u_{\text{FCC}}$), and LPG ($0.16 u_{\text{FCC}}$).
   - DHDS Feed: $u_{\text{DHDS}} \le \text{LGO} + \text{FCC\_LCO}$, yielding Ultra-Low Sulfur Diesel ($0.98 u_{\text{DHDS}}$) with $< 10$ ppm sulfur.
4. **Finished Product Blending Pools:**
   - LPG: $y_{\text{LPG}} = \text{LPG}_{\text{CDU}} + \text{LPG}_{\text{CCR}} + \text{LPG}_{\text{FCC}}$
   - Gasoline: $y_{\text{MS}} = b_{\text{LN\_MS}} + b_{\text{Ref\_MS}} + b_{\text{FCCGas\_MS}}$
   - Jet A-1 / ATF: $y_{\text{ATF}} = b_{\text{KERO\_ATF}}$
   - Diesel: $y_{\text{HSD}} = b_{\text{DHDS\_HSD}} + b_{\text{LGO\_HSD}}$
   - Fuel Oil: $y_{\text{FO}} = b_{\text{VR\_FO}} + b_{\text{AR\_FO}} + b_{\text{VGO\_FO}}$
5. **Quality Constraints:**
   - **Gasoline Research Octane Number (RON):**
     $$68 \cdot b_{\text{LN\_MS}} + 100 \cdot b_{\text{Ref\_MS}} + 92 \cdot b_{\text{FCCGas\_MS}} \ge \text{Target\_RON} \cdot y_{\text{MS}}$$
   - **Diesel Sulfur Specification (ppm):**
     $$8 \cdot b_{\text{DHDS\_HSD}} + 5000 \cdot b_{\text{LGO\_HSD}} \le \text{Max\_Sulfur} \cdot y_{\text{HSD}}$$
     *(Enforces that high-sulfur virgin LGO cannot bypass DHDS when BS-VI $\le 10$ ppm is in effect).*

---

## 3. Supported Industrial Scenarios

The digital twin includes five standard scenarios plus an infeasibility diagnostic stress test:

| Scenario Name | Description | Key Operational Characteristic |
|---|---|---|
| **Baseline** | Normal operating conditions (300 kbpd CDU). | Balanced crude basket; standard BS-VI demand and quality specs. |
| **High Demand** | Domestic transport fuel surge (+25% MS, +22% HSD). | CDU and conversion units driven to maximum utilization. |
| **Limited Crude** | Low-sulfur sweet crude supply disruption (-70% sweet availability). | Forces higher sour crude uptake; hydrotreaters (DHDS) operate at capacity. |
| **Unit Constraint** | Unplanned FCC turnaround (-50% FCC capacity). | Bottleneck shifts to gasoline upgrading; excess VGO redirected to Fuel Oil pool. |
| **Quality Constraint** | Premium export specification (RON 95, 8 ppm sulfur). | Severe blend tightening; eliminates quality giveaway margins. |
| **Infeasible Demand** | Stress test: HSD demand set to 350 kbpd (> 300 kbpd CDU limit). | Tests solver's ability to cleanly detect infeasibility and produce certificates. |

---

## 4. What-If Differential Analysis

The `RefineryTwin::compare_scenarios` engine computes differential metrics between baseline and scenarios:
- **Net Margin Impact:** Daily profit variation ($\Delta \$k/\text{day}$) and Gross Refining Margin ($\Delta \$/\text{bbl}$).
- **Crude Allocation Shift:** Quantitative change in crude purchasing mix.
- **Product Mix Shift:** Volumetric variations across LPG, MS, ATF, HSD, and Fuel Oil.
- **Bottleneck Dynamics:** Identifies newly emergent binding constraints (e.g., FCC capacity becoming tight) and relieved constraints.

---

## 5. Solver & Independent Verification Integration

1. **Model Generation:** The scenario configuration generates a pure `sankhya::Model`.
2. **Adaptive Execution:** SANKHYA evaluates problem characteristics (sparsity, dimensions, hardware) and routes execution to CPU or GPU.
3. **Trust Layer Verification:** The solution is audited from scratch by `sankhya::verify_solution`:
   - Primal feasibility verified on all rows and bounds.
   - Dual feasibility and KKT conditions verified.
   - Objective recomputed independently.
   - Integrality verified for MILP scenarios.
   - Backend execution integrity confirmed.

---

## 6. CLI Usage

The digital twin is accessible directly through the SANKHYA CLI:

```bash
# Run baseline scenario
sankhya refinery --scenario=baseline

# Run high demand scenario and compare against baseline
sankhya refinery --scenario=high_demand --compare

# Run unit constraint scenario with discrete MILP unit activation
sankhya refinery --scenario=unit_constraint --milp --compare

# Run quality constraint with quadratic penalty
sankhya refinery --scenario=quality_constraint --qp=0.05
```

---

## 7. Known Limitations

- **Linear Yield Approximations:** Assumes piecewise linear yields for distillation and secondary conversion. Non-linear blend index interactions (e.g. non-linear octane blending equations) are approximated using linear property indices.
- **Single-Period Steady State:** Formulated as a daily steady-state planning model; multi-period inventory dynamic trajectories are modeled separately in `bench/runners/generate_refinery_lp.py`.
