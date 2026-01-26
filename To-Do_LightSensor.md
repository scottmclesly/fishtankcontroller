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

Use a Moving Average Filter (size 10) to ignore noise from swimming fish. 


Store results in NVS (Non-Volatile Storage) for trend analysis.

Calibration Procedure:

Implement a CALIBRATE_OPTICAL_CLEAR command. This should be run by the user in a bucket of fresh, distilled water to set the "0 NTU" baseline.

Implement a CALIBRATE_OPTICAL_DIRTY command to be run just before a scheduled water change.

UI & Cloud Integration:

Modify the Local Web Interface to include a "Water Clarity" gauge. 


Add a Supabase Schema update to track turbidity_index and doc_index over time. 

Create a "Water Change Recommended" alert triggered when the doc_index exceeds a calibrated threshold. 


Safety & Error Handling:

Logic to abort sensing if the TSL2591 detects high ambient light (prevents sensor saturation and incorrect readings).