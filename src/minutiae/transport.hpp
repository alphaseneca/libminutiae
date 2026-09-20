#pragma once

#include <stdint.h>
#include <stddef.h>
#include <memory>
#include <string>

namespace minutiae {

// Abstract UART/Serial Transport Interface
// Decouples the device driver from OS or MCU architecture (Windows, Linux, Arduino, ESP-IDF, STM32)
class ITransport {
public:
    virtual ~ITransport() = default;
    virtual bool open(const std::string& port_name, uint32_t baud_rate) = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;
    virtual size_t write(const uint8_t* data, size_t len) = 0;
    virtual size_t read(uint8_t* buffer, size_t max_len, uint32_t timeout_ms) = 0;
    virtual void flush() = 0;
    virtual void purge_buffers() = 0;
};

// Factory helper to create an OS-specific serial transport (e.g. Win32 COM or POSIX tty)
std::unique_ptr<ITransport> create_serial_transport();

} // namespace minutiae
