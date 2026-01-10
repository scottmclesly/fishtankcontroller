1) Identify current implementation and failure mode

 Locate the ammonia calculation code path (search for: NH3, ammonia, TAN, unionized, fraction, pKa, Emerson, Bower, percent).

 Add temporary debug logging (or console prints) for:

 tempC, tempK

 pH

 tan_ppm (as entered)

 computed pKa

 computed fractionNH3 (0–1)

 computed %NH3

 final NH3_ppm

 Confirm which of these is happening:

 fraction computed > 1

 % multiplied twice

 TAN treated as NH₃ already

 ln/log10 mismatch

 Kelvin/Celsius mismatch

 wrong equation (saltwater used for freshwater)

2) Define correct input/output semantics (units contract)

 Confirm TAN input meaning in UI and storage:

 Treat TAN as mg/L (ppm) of NH₃–N as reported by most aquarium kits (common).

 If your test kit reports “NH₃/NH₄⁺ as NH₃”, decide and document which one you support.

 Define displayed derived metrics:

 nh3_fraction_percent (0–100)

 nh3_ppm (unionized ammonia concentration) — what users actually care about

 Add a “calculation assumptions” note in UI:

 “NH₃ is estimated from TAN + pH + temperature.”

3) Implement the correct freshwater NH₃ fraction formula

 Implement unionized ammonia fraction for freshwater:

 Convert temperature: tempK = tempC + 273.15

 Compute pKa using a known freshwater relationship (pick one and stick to it in docs/tests)

 Compute fraction:

 fractionNH3 = 1 / (1 + 10^(pKa - pH))

 Compute outputs:

 %NH3 = fractionNH3 * 100

 NH3_ppm = TAN_ppm * fractionNH3

 Ensure the code is deterministic and does not depend on EC/TDS/KH.

Acceptance criteria

 fractionNH3 is always within [0, 1]

 %NH3 is always within [0, 100]

 TAN = 0 → NH3_ppm = 0 and %NH3 = 0

4) Guardrails + validation (prevent “152%” ever appearing again)

 Hard clamp:

 fractionNH3 = clamp(fractionNH3, 0, 1)

 Add invalidation logic (preferred over silent clamp):

 If computed fraction is NaN/Inf or outside tolerance (e.g., < -0.01 or > 1.01), set:

 nh3_fraction_percent = null

 nh3_ppm = null

 raise an internal error flag for the UI

 Add UI behavior:

 If null/invalid → show “—” with tooltip: “Invalid calculation (check inputs).”

 Sanity constraints on input:

 pH allowed range: 0–14 (or 4–10 if you want to be strict)

 temp allowed range: 0–40°C (configurable)

 TAN allowed range: 0–20 ppm (configurable)

5) Fix UI naming & presentation (reduce user confusion)

 Rename tile:

 From: “NH₃ RATIO %”

 To: “Toxic NH₃ (ppm)” (primary) and optionally show “NH₃ fraction (%)” secondary

 Update settings panel text:

 “Total Ammonia Nitrogen (TAN) in ppm” → add clarification:

 “Enter TAN from your test kit (NH₃ + NH₄⁺).”

 Add warning label:

 “Derived metric — requires TAN input to be meaningful.”

6) Add unit tests + reference test vectors

 Add unit tests that run in CI for ammonia:

 Case A: pH=7.52, tempC=22.28, TAN=0 → NH3_ppm=0, %NH3=0

 Case B: same pH/temp, TAN=1.0 → %NH3 should be ~1–2% (range assertion), NH3_ppm ~0.01–0.02

 Case C: pH=8.2, tempC=28, TAN=1.0 → higher fraction (range assertion)

 Ensure tests assert:

 %NH3 <= 100

 fractionNH3 within [0,1]

 no NaN/Inf

7) Manual verification on your live tank UI

 Set TAN to 0 → confirm NH₃ ppm tile shows 0 (or “—” if you choose hide)

 Set TAN to a known test value (e.g., 0.25 ppm) → confirm NH₃ ppm becomes a small number (thousandths to hundredths)

 Confirm the old “152%” never appears even with weird inputs

8) Optional: Make TAN “optional” and safer by default

 If TAN is blank/unset:

 Hide NH₃ tiles OR show “Set TAN to enable”

 Add “last updated TAN” timestamp (so users remember it’s manual)

Deliverables

 Updated ammonia calculation module/function

 UI tile rename + units updated

 Validation + error handling

 Unit tests + test vectors

 Quick doc blurb in README/settings page explaining TAN vs NH₃

If you paste your current ammonia function (or the file name), I can rewrite it cleanly in one go and include the tests to match your stack.