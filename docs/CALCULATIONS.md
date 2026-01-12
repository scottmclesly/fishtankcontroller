# Water Quality Calculations

This document describes the scientific formulas and methodologies used to calculate derived water quality metrics from primary sensor readings.

## Overview

The Fish Tank Controller derives several important water quality parameters from the four primary measurements provided by the POET sensor:

**Primary Measurements (from POET sensor):**
- Temperature (°C)
- pH
- ORP (Oxidation-Reduction Potential, mV)
- EC (Electrical Conductivity, mS/cm)

**Derived Metrics (calculated):**
- TDS (Total Dissolved Solids, ppm)
- CO₂ (Dissolved Carbon Dioxide, ppm)
- NH₃ (Toxic Ammonia Fraction, %)
- NH₃ (Toxic Ammonia Concentration, ppm)
- Max DO (Maximum Dissolved Oxygen, mg/L)
- Stocking Density (cm/L)

---

## Total Dissolved Solids (TDS)

### Formula

```
TDS (ppm) = EC (μS/cm) × Conversion Factor
```

Where:
- **EC (μS/cm)** = Electrical Conductivity in microsiemens per centimeter
  - Convert from mS/cm: `EC (μS/cm) = EC (mS/cm) × 1000`
- **Conversion Factor** = Typically 0.5–0.7 (default: 0.64 for freshwater)

### Implementation

```cpp
float calculateTDS(float ec_ms_cm, float factor = 0.64) {
    return (ec_ms_cm * 1000.0) * factor;
}
```

### Background

Electrical conductivity measures the water's ability to conduct electricity, which is directly related to the concentration of dissolved ions. TDS represents the total mass of dissolved substances in the water.

**Conversion Factor Guidelines:**
- **0.50–0.55:** Very pure water with minimal dissolved minerals
- **0.64:** Standard freshwater aquarium (default)
- **0.67–0.70:** High mineral content or saltwater

### Interpretation

| TDS (ppm) | Water Type | Use Case |
|-----------|------------|----------|
| 0–50 | Distilled/RO | Dilution water, sensitive species |
| 50–150 | Soft water | South American biotopes, discus |
| 150–250 | Moderately soft | Most tropical fish |
| 250–400 | Moderately hard | African cichlids, livebearers |
| 400+ | Hard water | Rift lake cichlids, brackish |

---

## Dissolved Carbon Dioxide (CO₂)

### Formula

```
CO₂ (ppm) = 3.0 × KH (dKH) × 10^(7.0 - pH)
```

Where:
- **KH (dKH)** = Carbonate hardness in degrees (user-configured)
- **pH** = Current pH measurement
- **3.0** = Empirical constant for carbonate equilibrium

### Implementation

```cpp
float calculateCO2(float ph, float kh_dkh) {
    if (kh_dkh <= 0.0 || ph < 0.0 || ph > 14.0) return 0.0;

    float co2_ppm = 3.0 * kh_dkh * pow(10.0, (7.0 - ph));

    // Sanity check: limit to reasonable range
    if (co2_ppm > 100.0) co2_ppm = 100.0;
    if (co2_ppm < 0.0) co2_ppm = 0.0;

    return co2_ppm;
}
```

### Background

This formula is based on the **carbonate equilibrium system** in water:

```
CO₂ + H₂O ⇌ H₂CO₃ ⇌ HCO₃⁻ + H⁺ ⇌ CO₃²⁻ + 2H⁺
```

The relationship between CO₂, pH, and carbonate hardness follows from the Henderson-Hasselbalch equation applied to the carbonate buffer system.

### Interpretation

| CO₂ (ppm) | Status | Notes |
|-----------|--------|-------|
| 3–5 | Low | May limit plant growth |
| 6–14 | Normal | Suitable for most setups |
| 15–30 | High | Good for planted tanks with CO₂ injection |
| 30–50 | Very high | Risk of fish stress if O₂ is low |
| 50+ | Dangerous | Immediate action required |

**Note:** This calculation requires the user to configure **Carbonate Hardness (KH)** in the tank settings. Without this value, CO₂ cannot be calculated.

---

## Toxic Ammonia (NH₃) Fraction

### Formula

```
T_kelvin = temp_c + 273.15
pKa = 0.09018 + (2729.92 / T_kelvin)
NH₃_fraction = 1 / (10^(pKa - pH) + 1)
```

Where:
- **temp_c** = Water temperature in degrees Celsius
- **pH** = Current pH measurement
- **pKa** = Temperature-dependent dissociation constant
- **NH₃_fraction** = Fraction of total ammonia existing as toxic NH₃ (0.0–1.0)

### Implementation

```cpp
float calculateToxicAmmoniaRatio(float temp_c, float ph) {
    if (temp_c < 0.0 || temp_c > 50.0) return 0.0;
    if (ph < 0.0 || ph > 14.0) return 0.0;

    float t_kelvin = temp_c + 273.15;
    float pKa = 0.09018 + (2729.92 / t_kelvin);
    float fraction = 1.0 / (pow(10.0, (pKa - ph)) + 1.0);

    // Clamp to valid range
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;

    return fraction;
}
```

### Background

Ammonia exists in two forms in water:
- **NH₃** (un-ionized ammonia) – **Toxic** to fish
- **NH₄⁺** (ammonium ion) – **Non-toxic**

The ratio between these forms depends on **temperature** and **pH** according to the equilibrium:

```
NH₃ + H₂O ⇌ NH₄⁺ + OH⁻
```

This formula is based on **Emerson et al. (1975)** for freshwater ammonia dissociation.

### Interpretation

The fraction is expressed as a **percentage** in the UI (multiply by 100).

| NH₃ Fraction (%) | Temperature | pH | Risk Level |
|------------------|-------------|----|------------|
| < 1% | Low | < 7.5 | Safe (most ammonia as NH₄⁺) |
| 1–5% | Moderate | 7.5–8.0 | Monitor total ammonia |
| 5–20% | High | 8.0–9.0 | Significant toxicity concern |
| > 20% | Very high | > 9.0 | Extreme danger |

**Key Factors:**
- Higher **pH** → More toxic NH₃
- Higher **temperature** → More toxic NH₃
- Lower **pH** → More safe NH₄⁺

---

## Toxic Ammonia (NH₃) Concentration

### Formula

```
NH₃ (ppm) = TAN (ppm) × NH₃_fraction
```

Where:
- **TAN (ppm)** = Total Ammonia Nitrogen in ppm (user-configured)
- **NH₃_fraction** = Fraction calculated above (0.0–1.0)

### Implementation

```cpp
float calculateActualNH3(float total_tan_ppm, float toxic_ratio) {
    if (total_tan_ppm < 0.0 || toxic_ratio < 0.0) return 0.0;
    return total_tan_ppm * toxic_ratio;
}
```

### Background

This converts the **fraction** of toxic ammonia into an **actual concentration** by multiplying by the total ammonia present.

**Example:**
- Total Ammonia (TAN) = 1.0 ppm
- Toxic Fraction = 5% (0.05)
- Toxic NH₃ = 1.0 × 0.05 = **0.05 ppm**

### Interpretation

| NH₃ (ppm) | Toxicity Level | Action Required |
|-----------|----------------|-----------------|
| < 0.02 | Safe | No action |
| 0.02–0.05 | Low concern | Monitor, reduce feeding |
| 0.05–0.10 | Moderate concern | Water change recommended |
| 0.10–0.20 | High concern | Immediate water change |
| > 0.20 | Dangerous | Emergency action required |

**Note:** This calculation requires the user to configure **Total Ammonia Nitrogen (TAN)** in the tank settings. The POET sensor does NOT measure ammonia directly.

---

## Maximum Dissolved Oxygen (DO)

### Formula

```
DO (mg/L) = 14.652 - 0.41022×T + 0.007991×T² - 0.000077774×T³
```

Where:
- **T** = Temperature in degrees Celsius
- Polynomial coefficients derived from empirical oxygen solubility data

**For saltwater** (salinity correction):
```
Salinity Factor = 1.0 - (salinity_ppt × 0.002)
DO_saltwater = DO_freshwater × Salinity Factor
```

### Implementation

```cpp
float calculateMaxDO(float temp_c, float salinity_ppt = 0.0) {
    if (temp_c < 0.0 || temp_c > 50.0) return 0.0;

    // Polynomial for freshwater DO saturation
    float do_mg_l = 14.652
                    - (0.41022 * temp_c)
                    + (0.007991 * temp_c * temp_c)
                    - (0.000077774 * temp_c * temp_c * temp_c);

    // Salinity correction (if applicable)
    if (salinity_ppt > 0.0) {
        float salinity_factor = 1.0 - (salinity_ppt * 0.002);
        do_mg_l *= salinity_factor;
    }

    // Sanity check
    if (do_mg_l < 0.0) do_mg_l = 0.0;
    if (do_mg_l > 20.0) do_mg_l = 20.0;

    return do_mg_l;
}
```

### Background

Oxygen solubility in water decreases with increasing temperature and salinity. This formula calculates the **theoretical maximum** dissolved oxygen concentration at 100% saturation for the current conditions.

**Important:** This is NOT a measurement of actual DO – it shows the **maximum possible** DO if the water were fully saturated with oxygen.

### Interpretation

| Temperature (°C) | Max DO (mg/L) | Notes |
|------------------|---------------|-------|
| 10 | ~11.3 | Cold water, high capacity |
| 20 | ~9.1 | Room temperature |
| 25 | ~8.2 | Typical tropical aquarium |
| 30 | ~7.5 | Warm tropical, lower capacity |
| 35 | ~6.9 | Very warm, dangerously low |

**Actual DO Targets:**
- **Fish-only tanks:** 5–8 mg/L
- **Planted tanks:** 6–10 mg/L (higher with CO₂ injection)
- **Critical minimum:** ~4 mg/L (below this, fish are stressed)

**Why This Matters:**
- Warmer water holds **less oxygen** → Higher risk of hypoxia
- Saltwater holds **less oxygen** than freshwater
- Heavily stocked tanks need stronger aeration as temperature rises

---

## Stocking Density

### Formula

```
Stocking Density (cm/L) = Total Fish Length (cm) / Tank Volume (L)
```

Where:
- **Total Fish Length** = Sum of all fish body lengths in centimeters (user-configured)
- **Tank Volume** = Tank volume in liters (user-configured)

### Implementation

```cpp
float calculateStockingDensity(float total_fish_cm, float tank_volume_liters) {
    if (tank_volume_liters <= 0.0) return 0.0;
    if (total_fish_cm < 0.0) return 0.0;

    return total_fish_cm / tank_volume_liters;
}
```

### Background

This is a simplified "inch-per-gallon" (or cm-per-liter) rule for estimating bioload. While not scientifically precise (actual bioload depends on fish metabolism, waste production, filtration, etc.), it provides a useful guideline.

### Interpretation

| Density (cm/L) | Stocking Level | Requirements |
|----------------|----------------|--------------|
| < 0.5 | Very light | Minimal filtration |
| 0.5–1.0 | Light | Standard filtration |
| 1.0–2.0 | Moderate | Good filtration, regular water changes |
| 2.0–3.0 | Heavy | Strong filtration, frequent water changes |
| > 3.0 | Overstocked | High risk, expert care required |

**Color-Coded Warnings (UI):**
- **Green (< 1.0 cm/L):** Safe stocking level
- **Yellow (1.0–2.0 cm/L):** Moderate stocking, monitor water quality
- **Red (> 2.0 cm/L):** Heavy stocking, increase maintenance

**Limitations:**
- Does NOT account for fish species (1 cm of goldfish ≠ 1 cm of tetra in bioload)
- Does NOT consider body mass or feeding habits
- Does NOT account for filtration capacity
- Best used as a rough guideline, not absolute rule

**Note:** This calculation requires the user to configure **fish profiles** (species, lengths) and **tank volume** in the settings.

---

## Data Sources and References

### Primary Sensor (POET)

All derived metrics depend on accurate primary measurements from the Sentron POET sensor:

- **Temperature:** Measured directly, no calibration required (±0.5°C accuracy)
- **pH:** Requires calibration (1-point or 2-point) for accuracy
- **EC:** Requires cell constant calibration for accuracy
- **ORP:** Reference electrode dependent

See [CALIBRATION.md](CALIBRATION.md) for calibration procedures.

### User-Configured Parameters

Several derived metrics require user input:

| Metric | Required User Input | Location |
|--------|---------------------|----------|
| TDS | TDS Conversion Factor (default 0.64) | Tank Settings |
| CO₂ | Carbonate Hardness (KH, dKH) | Tank Settings |
| NH₃ | Total Ammonia Nitrogen (TAN, ppm) | Tank Settings |
| Max DO | Salinity (ppt, default 0 for freshwater) | Tank Settings |
| Stocking Density | Tank Volume (L), Fish Profiles (cm) | Tank Settings, Fish Profiles |

### Scientific References

- **TDS/EC Relationship:** Standard aquarium practice, conversion factors from water testing industry
- **CO₂ Calculation:** Carbonate equilibrium chemistry (Henderson-Hasselbalch equation)
- **Toxic Ammonia Ratio:** **Emerson et al. (1975)** – "Aqueous Ammonia Equilibrium Calculations: Effect of pH and Temperature"
- **Dissolved Oxygen Solubility:** Empirical polynomial fit to standard oxygen solubility tables
- **Stocking Density:** Traditional "inch-per-gallon" rule adapted to metric units

---

## Implementation Notes

### Code Location

All calculation functions are implemented in:
- **Header:** [lib/DerivedMetrics/DerivedMetrics.h](../lib/DerivedMetrics/DerivedMetrics.h)
- **Implementation:** [lib/DerivedMetrics/DerivedMetrics.cpp](../lib/DerivedMetrics/DerivedMetrics.cpp)

### Error Handling

All calculation functions include input validation:
- Return `0.0` for invalid/out-of-range inputs
- Clamp results to physically meaningful ranges
- Handle division by zero gracefully

### Units Consistency

| Parameter | Units | Storage | Display |
|-----------|-------|---------|---------|
| Temperature | °C | float | 1 decimal |
| pH | pH units | float | 2 decimals |
| EC | mS/cm | float | 3 decimals |
| TDS | ppm | float | 0 decimals |
| CO₂ | ppm | float | 1 decimal |
| NH₃ Fraction | fraction (0–1) | float | % (multiply by 100) |
| NH₃ Concentration | ppm | float | 3 decimals |
| Max DO | mg/L | float | 1 decimal |
| Stocking | cm/L | float | 2 decimals |

---

## Related Documentation

- [CALIBRATION.md](CALIBRATION.md) - Sensor calibration procedures
- [CONFIGURATION.md](CONFIGURATION.md) - Tank settings and fish profiles
- [WEB_UI.md](WEB_UI.md) - How metrics are displayed in the web interface
- [MQTT.md](MQTT.md) - Publishing derived metrics to MQTT/Home Assistant

---

**Last Updated:** 2026-01-11
**Firmware Version:** v0.9
