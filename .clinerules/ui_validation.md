Do NOT assume UI consistency is correct.

Perform a STRUCTURED UI consistency audit.

Use .clinerules/ui_consistency.md as the source of truth.

---

## VALIDATION TASK

Analyze layout code and report:

---

### 1. SPACING SYSTEM

* List ALL spacing values used

CHECK:

* Are values from the core token set (4, 8, 12, 16, 24, 32)?
* Are exceptions (6, 20, 28, 64) used only in correct scope?
* Are there any random values (e.g. 7, 9, 11, 13)?

---

### 2. NORMALIZATION

* Identify values that should be normalized:

Examples:

* 2 → 4
* 10 → 12
* 14 → 16
* 18 → 16
* 22 → 24
* 26 → 24

CHECK:

* Are there obvious outliers?
* Would normalization improve consistency?

---

### 3. COMPONENT SIZES

* List sizes of:

  * knobs
  * buttons
  * labels

CHECK:

* Are similar elements mostly consistent?
* Are variations intentional (hierarchy) or random?

---

### 4. ALIGNMENT

* Check alignment:

  * left edges
  * right edges
  * stacking

CHECK:

* Are elements aligned consistently?
* Any structural misalignment?

---

### 5. HIERARCHY

* Identify:

  * primary elements
  * secondary elements

CHECK:

* Do primary elements stand out?
* Is size/importance used intentionally?

---

### 6. GROUPING

* Identify UI groups

CHECK:

* Are related elements grouped visually?
* Is spacing between groups larger than inside groups?

---

## OUTPUT FORMAT

For each category:

* PASS / WARN / FAIL

Where:

* PASS = consistent
* WARN = minor issues or acceptable deviations
* FAIL = structural or system-breaking issues

---

## IMPORTANT

* Do NOT treat minor deviations as failures
* Do NOT suggest changes that break layout_rules
* Focus on patterns, not isolated values
* Respect INDIE MODE (practical over perfect)
