# UI Consistency System

## PART 1: PRINCIPLES

* Use a small set of spacing values
* Prefer consistency over randomness
* Reuse values instead of introducing new ones
* Similar elements should have similar sizes
* Maintain clear visual hierarchy (primary / secondary / tertiary)
* Align elements to shared edges
* Group related elements and separate groups with larger spacing

---

## PART 2: CORE TOKENS (default)

4, 8, 12, 16, 24, 32

---

## PART 3: EXCEPTIONS (scoped)

6  → chip/button micro gaps
20 → drop zone padding
28 → outer margin
64 → special one-off spacing

---

## PART 4: USAGE RULES

* Default → use core tokens
* One-off values allowed if they improve UI
* If value is reused 2+ times → promote to exception or token
* Avoid random values like 7, 9, 11, 13

---

## PART 5: NORMALIZATION

2  → 4
10 → 12
14 → 16
18 → 16
22 → 24
26 → 24

---

## PART 6: SIZE GUIDELINES

* Knobs: 62–86 (dynamic)
* Similar controls should have similar sizes
* Minor variation (±1–2px) is allowed
* Use size differences to express hierarchy

---

## PART 7: INDIE MODE

* No documentation required for one-off values
* System should not slow down development
* Use judgment over strict enforcement
* Do NOT refactor working UI without clear benefit
