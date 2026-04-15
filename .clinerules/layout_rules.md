# JUCE UI Layout Rules (STRICT)

## SCOPE OF RULES

These rules apply ONLY to layout code:

* resized()
* layout helper functions

These rules DO NOT apply to:

* paint()
* paintOverChildren()
* animation code
* visual transforms

---

## CORE PRINCIPLES

1. SINGLE LAYOUT PIPELINE

* All layout must start from getLocalBounds()
* Layout must flow top → down using removeFromTop/Left/Right/Bottom

2. NO ABSOLUTE POSITIONING

* Do NOT use manual X/Y positioning
* Do NOT calculate positions like:
  x = area.getWidth() - something
* Do NOT center elements using math

3. COMPONENT-BASED STRUCTURE

* UI must be divided into logical regions (header, waveform, toolbar, controls)
* Each component must receive a dedicated rectangle

4. RESIZED() IS THE SOURCE OF TRUTH

* All layout logic must live in resized()
* No layout logic in paint()

---

## ALLOWED

* Division for layout partitioning:
  auto slotW = area.getWidth() / 8;

* Size calculations:
  widths, heights, gaps, proportions

* Using calculated sizes to SLICE layout:
  auto slot = area.removeFromLeft(slotW);

* reduced() for padding and margins

---

## FORBIDDEN

* Using calculated values to POSITION elements manually:
  ❌ int x = (area.getWidth() - w) / 2;
  ❌ comp.setBounds(x, y, w, h);

* Centering via math

* Offset-based positioning (x + offset)

* withX, withRight, withCentre for layout

---

## RULE OF THUMB

GOOD:
auto slot = area.removeFromLeft(area.getWidth() / 4);

BAD:
int x = (area.getWidth() - 200) / 2;
comp.setBounds(x, ...);

---

## STRICT MODE

These rules are mandatory.
Any violation = incorrect implementation.
