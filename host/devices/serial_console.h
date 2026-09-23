#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace vmarea {

/// SerialConsole emulates a minimal COM1 serial port for guest-to-host output.
/// Phase 1 only handles data writes (TX) on the base port 0x3F8.
class SerialConsole {
public:
    static constexpr uint16_t COM1_BASE = 0x3F8;
    static constexpr uint16_t COM1_END  = 0x3FF;

    SerialConsole() = default;
    ~SerialConsole() = default;

    /// Handle an I/O port write from the guest.
    void handleWrite(uint16_t port, uint8_t data);

    /// Check if the given port belongs to this device (COM1 range).
    bool ownsPort(uint16_t port) const;

    /// Get all accumulated output text.
    const std::string& output() const;

    /// Clear the output buffer.
    void clear();

    /// Set a callback for real-time character output.
    using OutputCallback = std::function<void(char)>;
    void setOutputCallback(OutputCallback cb);

private:
    std::string buffer_;
    OutputCallback callback_;
};

} // namespace vmarea
