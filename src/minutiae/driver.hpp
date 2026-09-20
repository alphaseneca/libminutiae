#pragma once

#include "types.hpp"
#include "transport.hpp"
#include <string>

namespace minutiae {

struct DeviceInfo {
    std::string driver_name;
    std::string model_name;
    std::string chip_unique_id;
    uint16_t width_px = 0;
    uint16_t height_px = 0;
    uint16_t resolution_dpi = 0;
    uint16_t resolution_dpcm = 0;
    uint16_t security_level = 0;
    uint16_t capacity = 0;
};

// Abstract Hardware Fingerprint Device Driver
// Standardizes hardware interaction so new sensors can be added cleanly under drivers/
class IDeviceDriver {
public:
    virtual ~IDeviceDriver() = default;

    virtual const char* get_driver_name() const = 0;
    virtual const char* get_model_name() const = 0;

    virtual bool initialize(ITransport& transport) = 0;
    virtual void release() = 0;

    // Authentication: Read factory-burned silicon Unique ID
    virtual bool read_chip_unique_id(std::string& out_uid) = 0;

    // Query device parameters & geometry
    virtual bool get_device_info(DeviceInfo& out_info) = 0;

    // Presence detection
    virtual bool is_finger_present() = 0;

    // Image capture: triggers sensor optical/capacitive acquisition and transfers frame to host
    virtual bool capture_raw_image(RawImage& out_image) = 0;
};

} // namespace minutiae
