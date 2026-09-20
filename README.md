# libminutiae

[![Standard: ISO/IEC 19794-2](https://img.shields.io/badge/Standard-ISO%2FIEC%2019794--2-blue.svg)](https://www.iso.org/standard/50864.html)
[![Standard: ANSI INCITS 378](https://img.shields.io/badge/Standard-ANSI%20INCITS%20378-blue.svg)](https://standards.incits.org/)
[![Quality: NIST NFIQ 2](https://img.shields.io/badge/Quality-NIST%20NFIQ%202-green.svg)](https://www.nist.gov/services-resources/software/nfiq-2)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17-purple.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A lightweight, dependency-free C++17 biometric fingerprint identification, quality assessment, and ISO/ANSI standardization library. Bringing the driver-decoupled architecture of desktop stacks like `libfprint` to embedded firmware, **libminutiae** is engineered for resource-constrained microcontrollers (ESP32, STM32, Atmel SAMD) and desktop platforms alike.

---

## Technical Specifications

- **Runtime Dependencies**: None (self-contained C++17, zero external libraries).
- **Peak Working SRAM**: `< 25 KB` for $96 \times 100$ sensor frames (suitable for embedded systems with $\ge 32\text{ KB}$ SRAM).
- **Standards Implemented**:
  - **ISO/IEC 19794-2:2005** (`"FMR\0"`, version `" 20\0"`)
  - **ISO/IEC 19794-2:2011** (`"FMR\0"`, version `" 30\0"`)
  - **ANSI INCITS 378:2004** (`"FMR\0"`, version `" 20\0"`)
  - **ANSI INCITS 378:2009** (`"FMR\0"`, version `" 35\0"`)
- **Quality Metrics (NIST NFIQ 2 / ISO 29794-4)**:
  - Local Clarity Score (LCS) via ridge-valley contrast variance.
  - Orientation Certainty Level (OCL) via directional gradient coherence.
  - Minutiae spatial spread and spatial density analysis.
- **Minutiae Extraction**:
  - Adaptive gradient binarization.
  - Zhang-Suen morphological skeleton thinning.
  - Rutovitz Crossing Number ($CN=1$ for ridge endings, $CN=3$ for ridge bifurcations).
- **Matching & Coordinate Invariance**:
  - Physical scale normalization: Coordinates are transformed to metric micrometers ($\mu\text{m}$) using native sensor DPCM tags.
  - 1:1 metric spatial and angular matching.
  - Anti-replay protection against identical bitstream reinjection.
- **Hardware Abstraction**: Pluggable driver architecture (`minutiae::IDeviceDriver`) with transport decoupling (`minutiae::ITransport`). Encapsulates capacitive fingerprint sensor hardware (`minutiae::drivers::R558S`).

---

## Architecture

```
                          +-------------------------------+
                          |       Application Logic       |
                          +---------------+---------------+
                                          |
                          +---------------v---------------+
                          |       minutiae::Engine        |
                          +---+-----------+-----------+---+
                              |           |           |
             +----------------v---+   +---v----+  +---v------------------+
             | Feature Extraction |   | Matcher|  | Standards Encoder    |
             | (Zhang-Suen / CN)  |   | (1:1)  |  | (ISO 19794-2 / ANSI) |
             +--------------------+   +--------+  +----------------------+
                                          |
                          +---------------v---------------+
                          |    minutiae::IDeviceDriver    | (e.g. drivers::R558S)
                          +---------------+---------------+
                                          |
                          +---------------v---------------+
                          |      minutiae::ITransport     | (UART / SPI / USB)
                          +-------------------------------+
```

---

## API Reference

### `minutiae::Engine`

Header: `<minutiae.hpp>`

```cpp
namespace minutiae {

class Engine {
public:
    // Extract minutiae feature points from an 8-bit grayscale raw frame
    std::vector<Minutia> extract_minutiae(const RawImage& image, const ExtractionConfig& config = {});

    // Evaluate NIST NFIQ 2 / ISO 29794-4 biometric quality metrics
    NFIQ2Report evaluate_quality(const RawImage& image, const std::vector<Minutia>& minutiae);

    // Construct an in-memory template with normalized physical coordinates (micrometers)
    BiometricTemplate create_template(const RawImage& image, const std::vector<Minutia>& minutiae, FingerPosition position = FingerPosition::UNKNOWN);

    // 1:1 Biometric Verification between enrolled and candidate templates
    bool verify(const BiometricTemplate& enrolled, const BiometricTemplate& candidate, uint8_t& out_score, const MatcherConfig& config = {});

    // Adaptive minutiae self-learning (enriches enrolled template with reliable candidate points)
    bool adapt(BiometricTemplate& enrolled, const BiometricTemplate& candidate, uint8_t match_score);

    // Detect identical synthetic or bitstream-level replay attacks
    bool check_replay(const RawImage& current_frame, const RawImage& previous_frame);

    // Binary file persistence for standard ISO/ANSI Finger Minutiae Records (FMR)
    static bool save_fmr_file(const std::string& path, const BiometricTemplate& tmpl, StandardFormat format = StandardFormat::ISO_19794_2_2005);
    static bool load_fmr_file(const std::string& path, BiometricTemplate& out_tmpl);
};

} // namespace minutiae
```

---

### `minutiae::drivers::R558S`

Header: `<minutiae.hpp>` (or `<drivers/r558s/r558s.hpp>`)

Driver implementation for the GROW R558-S capacitive sensor module over UART (8N2, default 57600 baud):

```cpp
namespace minutiae {
namespace drivers {

class R558S : public IDeviceDriver {
public:
    explicit R558S(uint32_t device_address = 0xFFFFFFFF);

    // Device lifecycle
    bool initialize(ITransport& transport) override;
    void release() override;

    // Hardware status & inspection
    bool read_chip_unique_id(std::string& out_uid) override; // Reads 8-byte silicon UID (0x34)
    bool get_device_info(DeviceInfo& out_info) override;
    bool is_finger_present() override;

    // Image capture
    bool capture_raw_image(RawImage& out_image) override;    // Streams 4,800 bytes (96x100 8-bit)

    // LED ring control (PS_ControlBLN 0x3C)
    // Modes: BREATHING, FLASHING, ALWAYS_ON, OFF
    // Colors: BLUE, GREEN, CYAN, RED, PURPLE, YELLOW, WHITE
    bool set_led(LedMode mode, LedColor start_color, LedColor end_color = LedColor::NONE, uint8_t cycles = 0);

    // Hardware True Random Number Generator (0x14)
    bool get_hardware_random(uint32_t& out_random);

    // On-chip Match-on-Module database functions (optional sensor-side routines)
    bool auto_enroll(uint16_t page_id, uint8_t num_samples = 4);
    bool search_database(uint16_t& out_page_id, uint16_t& out_score);
    bool empty_database();
};

} // namespace drivers
} // namespace minutiae
```

---

## Developer Usage Examples

### 1. Minutiae Extraction, Standardization & 1:1 Verification

```cpp
#include <minutiae.hpp>
#include <iostream>

int main() {
    minutiae::Engine engine;

    // 1. Ingest an 8-bit grayscale frame (96x100 at 200 dpcm / ~508 DPI)
    minutiae::RawImage enroll_frame(96, 100, 200);
    // ... fill enroll_frame.data with sensor pixel bytes ...

    // 2. Extract minutiae points (ridge endings and bifurcations)
    auto minutiae_list = engine.extract_minutiae(enroll_frame);

    // 3. Quality estimation (NIST NFIQ 2)
    auto quality = engine.evaluate_quality(enroll_frame, minutiae_list);
    if (quality.overall_score < 40) {
        std::cerr << "Frame quality insufficient (" << (int)quality.overall_score << "/100)\n";
        return 1;
    }

    // 4. Create standard template and save as ISO/IEC 19794-2:2005 FMR binary record
    auto enrolled_tmpl = engine.create_template(enroll_frame, minutiae_list, minutiae::FingerPosition::RIGHT_INDEX);
    minutiae::Engine::save_fmr_file("enrolled.fmr", enrolled_tmpl, minutiae::standards::StandardFormat::ISO_19794_2_2005);

    // 5. Verification against a candidate scan
    minutiae::RawImage candidate_frame(96, 100, 200);
    // ... fill candidate_frame.data ...
    auto candidate_minutiae = engine.extract_minutiae(candidate_frame);
    auto candidate_tmpl = engine.create_template(candidate_frame, candidate_minutiae);

    uint8_t match_score = 0;
    if (engine.verify(enrolled_tmpl, candidate_tmpl, match_score)) {
        std::cout << "Match verified! Score: " << (int)match_score << "/100\n";
    } else {
        std::cout << "Access denied. Score: " << (int)match_score << "/100\n";
    }

    return 0;
}
```

---

### 2. Live Sensor Ingestion (GROW R558-S)

```cpp
#include <minutiae.hpp>
#include <iostream>

int main() {
    // Open UART transport (8N2, 57600 baud)
    auto transport = minutiae::create_serial_transport();
    if (!transport->open("COM6", 57600)) {
        std::cerr << "Failed to open serial port.\n";
        return 1;
    }

    minutiae::drivers::R558S sensor;
    if (!sensor.initialize(*transport)) {
        std::cerr << "Failed to communicate with sensor.\n";
        return 1;
    }

    // Set LED to breathing Blue while waiting for finger touch
    sensor.set_led(minutiae::drivers::LedMode::BREATHING, minutiae::drivers::LedColor::BLUE);

    if (sensor.is_finger_present()) {
        minutiae::RawImage raw;
        if (sensor.capture_raw_image(raw)) {
            // Signal success: flash Green 3 times
            sensor.set_led(minutiae::drivers::LedMode::FLASHING, minutiae::drivers::LedColor::GREEN, minutiae::drivers::LedColor::GREEN, 3);

            // Process image with minutiae::Engine...
            minutiae::Engine engine;
            auto points = engine.extract_minutiae(raw);
            std::cout << "Captured frame with " << points.size() << " minutiae points.\n";
        }
    }

    sensor.release();
    transport->close();
    return 0;
}
```

---

## Resource Profile & Benchmarks

Benchmarked on **ESP32 (Tensilica Xtensa Dual-Core @ 160 MHz)** and **STM32F401 (ARM Cortex-M4 @ 84 MHz)** processing a $96 \times 100$ raw frame:

| Pipeline Stage | Working Memory (SRAM) | Execution Latency (160 MHz) |
| :--- | :--- | :--- |
| Gradient Binarization | In-place / 1 buffer (< 9.6 KB) | ~11 ms |
| Zhang-Suen Thinning | 1-bit packed scratchpad (< 1.5 KB) | ~26 ms |
| Crossing Number Minutiae Extraction | Stack allocated (< 1 KB) | ~3 ms |
| NIST NFIQ 2 Quality Computation | ~4 KB | ~8 ms |
| 1:1 Metric Verification | < 1 KB | ~1.6 ms |
| **Total Peak SRAM** | **~19.7 KB** | — |

---

## Build Instructions

### CMake
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Direct GCC Compilation
```bash
g++ -std=c++17 -Wall -Wextra -Werror -O2 -I src \
    src/core/*.cpp \
    src/drivers/r558s/*.cpp \
    src/transport/*.cpp \
    examples/01.BasicSelfTest/basic_self_test.cpp \
    -o basic_self_test
```

### Arduino / PlatformIO
Add `libminutiae` to your Arduino `libraries/` directory or reference in `platformio.ini`:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
build_flags = -std=gnu++17
```

---

## License

This project is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for complete details.
