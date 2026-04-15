Do NOT assume the layout is correct.

Perform a STRICT self-audit against the layout rules.

---

## SCOPE OF VALIDATION

Only validate:

* resized()
* layout helper functions

DO NOT scan:

* paint()
* paintOverChildren()
* animation code
* rendering logic

---

## VALIDATION TASK

1. Scan ALL layout code (resized() and helpers)

2. For EACH rule, report:

---

### RULE 1: No absolute positioning

Check for:

* setBounds(x, y, ...)
* manual X/Y calculations
* width/height math used for positioning

---

### RULE 2: Single layout pipeline

* Confirm layout starts from getLocalBounds()
* Confirm layout flows via removeFrom operations
* Identify any breaks

---

### RULE 3: No centering math

Search for:

* (area.getWidth() - X) / 2
* (area.getHeight() - Y) / 2

IMPORTANT:
Division for partitioning (width / N) is ALLOWED.

---

### RULE 4: No implicit positioning

Check for:

* withX, withRight, withCentre
* index-based positioning (i * width)
* lambdas calculating positions

---

### RULE 5: Helper functions

Inspect ALL helpers:

* Ensure they do NOT contain hidden positioning math

---

## OUTPUT FORMAT

* "PASS" or "FAIL" per rule
* If FAIL → show exact code snippet and line
* No summaries without evidence

---

## IMPORTANT

* Layout is valid ONLY if ALL rules PASS
* Do NOT say "mostly correct"
* Do NOT flag allowed division math
