# Ramped Stepper Motion Algorithm (β + Bisection Version)

**Goal:** Generate a sequence of step pulses for a stepper motor following a ramp profile using a β-scaling factor computed via bisection to match both total steps and total time.

---

## Inputs

- `profile[N]` — array of gains `g[i]` ∈ (0,1], with `g[0] = 1` (defines relative period shape)  
- `periods[N]` — nominal period per segment (µs)  
- `totalSteps` — total number of steps to execute  
- `totalTimeUs` — desired total move duration (µs)  
- Optional: `T_min` — minimal period allowed by hardware  

---

## Algorithm Overview

1. **Compute weight vector w(β)**  
   For a given scaling factor `β`:

   ```text
   w[i] = g[i]^(-β)
   W = sum_{i=0..N-1} w[i]
   ```

2. **Compute segment times**  
   Each segment is allocated a proportion of total time based on w[i]:

   ```text
   Tseg[i] = totalTimeUs * w[i] / W
   ```

3. **Compute produced steps Sprod(β)**  
   ```text
   Sprod = sum_{i=0..N-1} (Tseg[i] / periods[i])
   ```

4. **Bisection search for β**  
   - Initialize `β_low` and `β_high` bounds.  
   - Iterate until `|Sprod(β) - totalSteps| < ε` or max iterations reached:
     1. β_mid = (β_low + β_high)/2
     2. Compute Sprod(β_mid)
     3. If Sprod < totalSteps → decrease β (β_high = β_mid)
     4. If Sprod > totalSteps → increase β (β_low = β_mid)

5. **Assign steps and periods per segment**  
   After convergence:

   ```text
   Tseg[i] = totalTimeUs * w[i] / W
   S_i = floor(Tseg[i] / periods[i])
   ```

6. **Feasibility Gate (cheap check)**  
   ```text
   S_total ∈ [ totalTimeUs / T_min , totalTimeUs / max(periods) ]
   ```
   - Reject immediately if outside this range.

7. **Result**  
   - Output `segments[N]` containing `{ period, steps }` ready for pulse generation.

---

## Notes

- Bisection ensures **exact total steps** without violating the profile shape.  
- Preserves **monotonicity and relative profile scaling**.  
- Can handle **non-uniform segment times** naturally.  
- Works for **full-step or microstepped motion**.  
- Optional: enforce hardware limits via `T_min` and max period checks.