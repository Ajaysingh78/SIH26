# Phase 2 Feature 7 — Industrial Decision Intelligence

## 1. Executive Purpose

The **Industrial Decision Intelligence** layer transforms raw, verified mathematical optimization outputs into clear, deterministic, solver-derived industrial insights and operational recommendations.

```
OPTIMIZATION RESULT
       ↓
UNDERSTAND RESULT
       ↓
IDENTIFY CONSTRAINTS
       ↓
IDENTIFY BOTTLENECKS
       ↓
ANALYZE RESOURCE UTILIZATION
       ↓
COMPARE SCENARIOS
       ↓
GENERATE ACTIONABLE INDUSTRIAL INSIGHTS
```

### Strict Non-Negotiables
- **NOT an AI Chatbot / LLM:** No generative AI, language models, or probabilistic hallucination are used anywhere in the decision intelligence loop.
- **Strictly Solver-Derived:** Every insight, diagnostic, and recommendation is calculated directly from actual model matrices, primal activities, dual shadow prices, slack variables, KKT residuals, and verified what-if differentials.
- **Verified-Result Gate:** Unverified or failed solver results are strictly blocked from generating authoritative operational recommendations.
- **Decision Support, Not Replacement:** The system provides deterministic decision support; it **does not replace human operational judgment**, licensed refinery engineering standards, or safety shutdown protocols.

---

## 2. Input Data & Information Available

The Decision Intelligence layer consumes verified outputs from the preceding SANKHYA pipeline:

| Pipeline Layer | Input Data Consumed | Role in Decision Intelligence |
|---|---|---|
| **Phase 1: SANKHYA Core** | Primal solution $x^*$, objective $z^*$, basis status, simplex iterations, runtime | Quantifies physical throughputs, product volumes, and financial margins. |
| **Phase 2 Feature 1: GPU Foundation** | Backend device telemetry, kernel timing | Hardware execution context. |
| **Phase 2 Feature 2: Adaptive Router** | Problem classification (LP/QP/MILP), matrix nonzeros, sparsity %, routing rationale | Explains computational path and formulation complexity. |
| **Phase 2 Feature 3: Independent Trust Layer** | Independent primal residual $\|A x - b\|_\infty$, dual feasibility $\|A^T y - c\|_\infty$, tolerance $\epsilon$, certificate | Governs the **Verified-Result Gate**; unlocks authoritative operational status. |
| **Phase 2 Feature 4: MRPL Refinery Twin** | Unit capacities, crude availability quotas, product demands, blend quality limits (RON, sulfur) | Ground truth for physical ceilings, utilization %, and binding constraints. |
| **Phase 2 Feature 5: Industrial Dashboard** | Live REST endpoints (`/api/optimize`, `/api/compare`, `/api/decision-intelligence`) | Visualizes decisions, bottlenecks, and recommendations. |
| **Phase 2 Feature 6: Benchmark Laboratory** | Reproducibility evidence, verified run metrics | Separates operational advice from algorithmic benchmark evidence. |

---

## 3. The Verified-Result Gate (Task 4)

Decision insights are only as dependable as the numerical solution behind them. The system enforces a strict three-tier audit gate:

```
[Solver Result + Trust Layer Verification]
                 ↓
      Is Status == OPTIMAL?
        /              \
      YES              NO
      ↓                 ↓
Is Residual <= 1e-7?  Is Status == INFEASIBLE?
   /         \              /          \
 YES         NO           YES          NO
  ↓           ↓            ↓            ↓
[TIER 1]   [TIER 3]     [TIER 2]     [TIER 3]
VERIFIED    AUDIT        VERIFIED     UNVERIFIED
OPTIMAL    REJECTED     INFEASIBLE   NON-OPTIMAL
```

### Tier 1: `VERIFIED OPTIMAL`
- **Condition:** `solver_status == "OPTIMAL"` and `verification.is_trustworthy == True` (residuals $\le 10^{-7}$).
- **Gate Action:** `is_authoritative = True`.
- **Operational Status:** Authoritative industrial guidance is unlocked. Real operational debottlenecking and procurement recommendations are generated.

### Tier 2: `VERIFIED INFEASIBLE`
- **Condition:** `solver_status == "INFEASIBLE"` with dual Farkas ray proof ($b^T y < 0$).
- **Gate Action:** `is_authoritative = True` (authoritative diagnosis of physical impossibility).
- **Operational Status:** Plant dispatch recommendations are **strictly blocked**. Commercial renegotiation or force majeure declaration is recommended with the exact mathematical deficit identified (e.g., 350 kbpd diesel demand vs. 300 kbpd distillation ceiling).

### Tier 3: `AUDIT REJECTED / NON-AUTHORITATIVE`
- **Condition:** Solution residual $> 10^{-7}$, solver iteration limit reached, or non-optimal termination.
- **Gate Action:** `is_authoritative = False`.
- **Operational Status:** Authoritative recommendations are **withheld**. Setpoints are flagged with `AUDIT_HOLD`. No non-optimal solution is ever presented as definitive optimal guidance.

---

## 4. Bottleneck Diagnostic Methodology (Task 5)

A constraint or resource is categorized as a bottleneck **only when model data mathematically establishes it**. The diagnostic engine classifies bottlenecks into four distinct industrial categories:

### 1. Unit Capacity Ceilings (`UNIT_CAPACITY`)
- **Metric:** Utilization percentage $U_i = \frac{\text{Throughput}_i}{\text{Capacity}_i} \times 100\%$.
- **Critical (Binding):** $U_i \ge 99.5\%$. The unit operates at its physical ceiling ($s_i \approx 0$).
  - *Example:* Atmospheric Distillation (CDU) at 300.0/300.0 kbpd (100.0%) in Baseline.
  - *Example:* Fluid Catalytic Cracker (FCC) at 32.5/32.5 kbpd (100.0%) in Unit Constraint scenario.
- **Near-Binding:** $95.0\% \le U_i < 99.5\%$. Flagged as an operational vulnerability that would bottleneck under slight feed expansion.
- **Headroom:** $U_i < 70.0\%$. Identified as available conversion slack.

### 2. Feedstock Basket Scarcity (`FEEDSTOCK_AVAILABILITY`)
- **Metric:** Crude allocation vs. maximum contracted lifting quota.
- **Diagnostic:** When lifting quota reaches 100.0% ($s_c \approx 0$), the optimizer is starved of that crude grade.
  - *Example:* In Limited Crude, sweet crudes are curtailed by 70%, exhausting all available low-sulfur feedstock.

### 3. Product Demand Ceilings (`DEMAND_CAP`)
- **Metric:** Finished product yield vs. maximum commercial absorption capacity.
- **Diagnostic:** When product production equals max market demand ($P_j = D_{\max, j}$), additional capacity cannot be monetized without commercial off-take expansion.

### 4. Environmental & Quality Specifications (`QUALITY_SPEC`)
- **Metric:** Finished fuel quality blending constraints (octane number, sulfur concentration).
- **Diagnostic:** Evaluated from pool blending slacks. When LGO bypass to pool is 0.0 kbpd, 100% of stream volume is forced through severe hydrotreating (DHDS), limiting heavy crude run rate.

---

## 5. Scenario Intelligence & What-If Differentials (Task 6)

The engine provides quantitative differential intelligence comparing any operational scenario against the MRPL Baseline:

$$\Delta \text{Margin} = \text{Margin}_{\text{scenario}} - \text{Margin}_{\text{baseline}}$$
$$\Delta \text{Throughput}_i = \text{Throughput}_{i, \text{scenario}} - \text{Throughput}_{i, \text{baseline}}$$

### Deterministic Findings Across Standard MRPL Scenarios

| Scenario | Objective Delta ($\Delta Z$) | Physical Root Cause | Solver-Derived Decision Finding |
|---|---|---|---|
| **High Demand** | $+\$0.00$ /day ($0.0\%$) | CDU 300 kbpd ceiling is binding. | Higher market demand cannot increase volume because CDU is 100% saturated. Refinery maintains optimal crack spread without capacity creep. |
| **Limited Crude** | $-\$149.82\text{k}$ /day ($-6.9\%$) | Sweet crude imports cut by 70%. | Total crude charge drops from 300 to 242 kbpd ($-58.0$ kbpd). Operating regime shifts to Feedstock Starved; commercial recommendation urges spot crude procurement. |
| **Unit Constraint** | $-\$463.13\text{k}$ /day ($-21.3\%$) | FCC turnaround derates capacity to 32.5 kbpd. | Bottleneck shifts from CDU to FCC. 10.7 kbpd unconverted VGO is forced into discounted Fuel Oil pool. High-priority recommendation advises turnaround acceleration. |
| **Quality Constraint** | $-\$1,109.95\text{k}$ /day ($-51.0\%$) | BS-VI specs tightened to 95 RON MS and 8 ppm HSD. | Total crude intake throttled to 206.7 kbpd ($-93.3$ kbpd) to meet pool sulfur and octane limits. Octane giveaway and 100% LGO hydrotreating compress gross refining margin. |
| **Infeasible Demand** | Deficit ($-\infty$) | Diesel demand of 350 kbpd exceeds 300 kbpd CDU. | Infeasibility proven by dual Farkas ray. Operating dispatch is blocked; immediate commercial renegotiation is recommended. |

---

## 6. Actionable Recommendation Generation (Task 3 & 7)

Recommendations are generated through deterministic predicate rules based on actual mathematical indicators:

1. **Gate Verification Rule:** If solution is unverified, output `AUDIT_HOLD` recommendation.
2. **Infeasibility Rule:** If Farkas ray is active, recommend commercial volume reduction with exact deficit quantified.
3. **CDU Expansion Rule:** If CDU utilization $\ge 99.5\%$, identify positive shadow price and recommend pre-heat train debottlenecking.
4. **Turnaround Recovery Rule:** If FCC derating forces VGO diversion, calculate exact margin loss ($-\$463.13\text{k}$/day) and recommend maintenance acceleration.
5. **Feedstock Rebalancing Rule:** If $\ge 3$ crudes are exhausted, recommend procurement of incremental low-sulfur crudes.
6. **Quality Compliance Rule:** If quality specs are binding, recommend reformer severity adjustment and hydrotreater catalyst regeneration.

---

## 7. Dashboard & Benchmark Integration (Tasks 8 & 9)

- **Industrial Dashboard (`web-ui`):**
  - Displays the **Audit Gate Status Badge** (`VERIFIED OPTIMAL`, `VERIFIED INFEASIBLE`, `AUDIT REJECTED`).
  - Presents an **Executive Decision Summary** box with primary finding, operating regime, and key action.
  - Features a **Three-Column Intelligence Grid**:
    1. *Active Bottlenecks & Capacity Ceilings* (severity badges, % load, industrial interpretation).
    2. *Commercial Production Yields* (fulfillment status, shadow price, revenue contribution).
    3. *Resource Utilization & Feedstock Slate* (exhausted crudes, conversion unit loadings).
  - Lists **Actionable Recommendations** with priority flags (`IMMEDIATE`, `HIGH`, `MEDIUM`), mathematical basis, and expected impact.
  - Linked seamlessly to Step 7 (`INSIGHTS`) of the Industrial Workflow Stepper.
- **Benchmark Integration:**
  - Maintains strict separation between **Operational Decisions** and **Performance Evidence** (backend used, solve time in ms, simplex iterations, matrix dimensions, sparsity %).

---

## 8. Safety, Honesty, and Operational Scope (Task 10 & 12)

### Principles of Veracity
- **Zero Fabrication:** No arbitrary savings, inflated efficiencies, or ungrounded bottleneck claims.
- **Truthful Status Representation:** A feasible solution is never described as optimal unless proven by KKT optimality conditions ($\le 10^{-7}$). A non-optimal solution is never presented as definitive industrial advice.
- **Bounded Causality:** Causal explanations are restricted to direct mathematical mechanisms (e.g., derating FCC capacity limits VGO cracking, diverting flow to Fuel Oil).

### Scope and Human Operational Role
> [!IMPORTANT]
> The Industrial Decision Intelligence engine is an automated mathematical decision-support system. It computes optimal setpoints and constraint diagnostics based on the supplied mathematical formulation. It does **not** replace licensed refinery operations engineers, plant safety shutdown systems (SIS/SIL), or statutory environmental oversight. Refinery operators must evaluate all recommendations against physical plant integrity and operating guidelines prior to manual or supervisory dispatch.
