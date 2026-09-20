#pragma once

#include "../../minutiae/driver.hpp"
#include "../../minutiae/types.hpp"
#include <vector>

namespace minutiae {
namespace drivers {

// LED Modes (GROW R558-S PS_ControlBLN 0x3C)
enum class LedMode : uint8_t {
    BREATHING  = 1,
    FLASHING   = 2,
    ALWAYS_ON  = 3,
    OFF        = 4,
    GRADUAL_ON = 5,
    GRADUAL_OFF = 6
};

// LED Colors (Exact Synochip / GROW hardware registers)
enum class LedColor : uint8_t {
    NONE   = 0x00,
    BLUE   = 0x01, // 0x01: Pure Blue
    GREEN  = 0x02, // 0x02: Pure Green (Success / Verified)
    CYAN   = 0x03, // 0x03: Cyan / Light Blue
    RED    = 0x04, // 0x04: Pure Red (Failure / Rejected)
    PURPLE = 0x05, // 0x05: Purple
    YELLOW = 0x06, // 0x06: Yellow
    WHITE  = 0x07  // 0x07: White
};

// GROW R558-S Capacitive Fingerprint Module Driver
// Native Specs: 96x100 pixels, 508 DPI (200 dpcm), 4-bit packed UART stream (4,800 bytes)
class R558S : public IDeviceDriver {
public:
    explicit R558S(uint32_t device_address = 0xFFFFFFFF);
    ~R558S() override;

    // IDeviceDriver Implementation
    const char* get_driver_name() const override { return "GROW_R558S"; }
    const char* get_model_name() const override { return "GROW R558-S Capacitive"; }

    bool initialize(ITransport& transport) override;
    void release() override;

    // Hardware Identification: Reads factory-burned 8-byte Silicon Serial Number (0x34)
    bool read_chip_unique_id(std::string& out_uid) override;

    bool get_device_info(DeviceInfo& out_info) override;
    bool is_finger_present() override;

    // Captures image on sensor (0x01) and streams 4,800 bytes over UART (0x0A),
    // unpacking into a 96x100 8-bit grayscale RawImage
    bool capture_raw_image(RawImage& out_image) override;

    // Hardware-Specific Features:
    // LED Ring Control (0x3C)
    bool set_led(LedMode mode, LedColor start_color, LedColor end_color = LedColor::NONE, uint8_t cycles = 0);

    // Hardware True Random Number Generator (0x14)
    bool get_hardware_random(uint32_t& out_random);

    // Sensor On-Chip Match & Enrollment (Match-on-Module)
    bool auto_enroll(uint16_t page_id, uint8_t num_samples = 4);
    bool search_database(uint16_t& out_page_id, uint16_t& out_score);
    bool empty_database();

private:
    ITransport* m_transport = nullptr;
    uint32_t m_address = 0xFFFFFFFF;

    bool send_packet(uint8_t pid, const uint8_t* payload, size_t len);
    bool receive_ack(uint8_t& out_rc, std::vector<uint8_t>& out_payload, uint32_t timeout_ms = 1000);
    bool receive_data_stream(std::vector<uint8_t>& out_raw, uint32_t timeout_ms = 2500);
};

} // namespace drivers
} // namespace minutiae
