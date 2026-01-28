We are adding a feature to our Fish Tank Controller based on an ESP32-C6 (XIAO) using ESP-IDF. The hardware includes a TSL2591 light sensor ($I^2C$ 0x29) and a WS2812B RGB LED (GPIO 1). They are physically separated by 47.757mm.
Objective:
Implement an Active Optical Sensing Service to derive Water Turbidity and Water Age (DOC) metrics by pulsing the RGB LED during "lights out" periods and measuring reflectance.
Tasks:
Research & Algorithm Design:
Research Nephelometric Turbidity Unit (NTU) calculation using RGB backscatter.
Develop a ratio-metric analysis algorithm: Compare Blue (450nm) vs. Red (630nm) reflectance to determine water "yellowing" (DOC index).
Firmware Implementation (ESP-IDF):
Create a void measure_water_clarity() function.
Sequence:
Turn off all external tank lights (via 4-Channel Relay 0x7F).
Take a "Dark Floor" baseline reading with the TSL2591.
Pulse the WS2812B Green LED at 50% brightness; sample TSL2591.
Pulse Blue and Red sequentially; sample TSL2591.
Use a Moving Average Filter (size 10) to ignore noise from swimming fish. 1111
Store results in NVS (Non-Volatile Storage) for trend analysis.
Calibration Procedure:
Implement a CALIBRATE_OPTICAL_CLEAR command. This should be run by the user in a bucket of fresh, distilled water to set the "0 NTU" baseline.
Implement a CALIBRATE_OPTICAL_DIRTY command to be run just before a scheduled water change.
UI & Cloud Integration:
Modify the Local Web Interface to include a "Water Clarity" gauge. 2222
Add a Supabase Schema update to track turbidity_index and doc_index over time. 3
Create a "Water Change Recommended" alert triggered when the doc_index exceeds a calibrated threshold. 4444
Safety & Error Handling:
Logic to abort sensing if the TSL2591 detects high ambient light (prevents sensor saturation and incorrect readings).