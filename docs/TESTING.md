# Testing Guide

This document describes how to run tests for the Fishtank Controller firmware.

## Test Environments

The project uses two test environments:

### 1. Native Tests (Host Computer)

Native tests run on your development computer without hardware. These test pure C calculation functions that have no ESP-IDF dependencies.

**Components tested:**
- `derived_metrics` - TDS, CO2, ammonia, dissolved oxygen, stocking density calculations

**Run native tests:**
```bash
pio test -e native
```

**Expected output:**
```
27 test cases: 27 succeeded
```

### 2. Embedded Tests (ESP32-C6)

Embedded tests run on the actual ESP32-C6 device. These test components that require ESP-IDF functionality (NVS, FreeRTOS, etc.).

**Components tested:**
- `warning_manager` - Threshold evaluation, warning states
- `data_history` - Circular buffer operations, statistics
- `calibration` - pH/EC calibration storage and calculation

**Run embedded tests:**
```bash
# Connect your ESP32-C6 device first
pio test -e esp32c6_test
```

## Test Files

```
test/
├── README                           # PlatformIO test documentation
├── test_derived_metrics.cpp.arduino_bak  # Old Arduino test (archived)
└── test_native/
│   └── test_derived_metrics.c       # Native derived_metrics tests
└── test_embedded/
    └── test_components.c            # Embedded component tests
```

## Memory Monitoring

The firmware includes built-in memory monitoring for stability profiling:

- Logs heap usage every 60 seconds
- Tracks minimum free heap since boot
- Warns if free heap drops below 20KB
- Detects potential memory leaks (>50KB decrease)

**Monitor output example:**
```
I (60000) fishtank_main: Heap: free=250000, min_ever=220000, session_min=235000, change=+15000
I (60000) fishtank_main: Internal: free=180000, largest_block=160000
```

## Running Tests

### Quick Test (Native Only)
```bash
# Fast - runs on host, no hardware needed
pio test -e native
```

### Full Test (Native + Embedded)
```bash
# Requires ESP32-C6 connected
pio test
```

### Specific Test Filter
```bash
# Run only tests matching a pattern
pio test -e native -f "test_tds*"
```

## Writing New Tests

### Native Tests

Create tests for pure C functions in `test/test_native/`:

```c
#include <stdio.h>
#include <unity.h>
#include "your_header.h"

void setUp(void) { }
void tearDown(void) { }

void test_your_function(void) {
    TEST_ASSERT_EQUAL(expected, your_function(input));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_your_function);
    return UNITY_END();
}
```

### Embedded Tests

Create tests for ESP-IDF components in `test/test_embedded/`:

```c
#include "unity.h"
#include "nvs_flash.h"
#include "your_component.h"

void setUp(void) { }
void tearDown(void) { }

void test_your_component(void) {
    esp_err_t ret = your_component_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void app_main(void) {
    nvs_flash_init();
    UNITY_BEGIN();
    RUN_TEST(test_your_component);
    UNITY_END();
}
```

## Test Coverage

| Component | Coverage | Test Type |
|-----------|----------|-----------|
| derived_metrics | Complete | Native |
| calibration | Basic | Embedded |
| warning_manager | Complete | Embedded |
| data_history | Complete | Embedded |
| poet_sensor | Manual only | - |
| wifi_manager | Manual only | - |
| mqtt_manager | Manual only | - |
| http_server | Manual only | - |
| display_driver | Manual only | - |
| ota_manager | Manual only | - |

## Stability Testing

For long-term stability testing:

1. Build and flash the firmware:
   ```bash
   pio run -e seeed_xiao_esp32c6 -t upload
   ```

2. Monitor serial output:
   ```bash
   pio device monitor
   ```

3. Run for 48+ hours and check:
   - Heap usage remains stable (check "Heap:" logs)
   - No crash or restart messages
   - Sensor readings continue at expected intervals
   - WiFi/MQTT reconnections succeed

## Troubleshooting

### Native test errors
- Ensure you have a C compiler installed (gcc/clang)
- Check that the test file uses `.c` extension for C tests

### Embedded test timeout
- Verify device is connected and port is correct
- Try `pio device list` to check available ports
- Reset the device and retry

### Memory warnings
- Check for memory leaks if heap consistently decreases
- Review recent changes for malloc without free
- Consider reducing buffer sizes if heap is too low
