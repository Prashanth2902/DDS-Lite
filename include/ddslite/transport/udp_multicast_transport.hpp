#pragma once

// Windows-only (Winsock2). Cross-platform support is deferred until a
// second platform actually needs it.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>

namespace ddslite::transport {

namespace detail {
class WinsockContext;
}

// Thrown on any unrecoverable Winsock failure. Carries the raw
// WSAGetLastError() code alongside a human-readable message.
class transport_error : public std::runtime_error {
public:
    transport_error(std::string_view what, int wsa_error_code);

    int wsa_error_code() const noexcept { return wsa_error_code_; }

private:
    int wsa_error_code_;
};

// A single UDP multicast socket that can both send to and receive from
// the group it joins on construction. Move-only: wraps a raw SOCKET
// handle, so copying would double-close it.
class UdpMulticastTransport {
public:
    UdpMulticastTransport(std::string_view multicast_group_ip,
                          std::uint16_t port,
                          std::string_view local_interface_ip = "0.0.0.0");
    ~UdpMulticastTransport();

    UdpMulticastTransport(const UdpMulticastTransport&) = delete;
    UdpMulticastTransport& operator=(const UdpMulticastTransport&) = delete;
    UdpMulticastTransport(UdpMulticastTransport&& other) noexcept;
    UdpMulticastTransport& operator=(UdpMulticastTransport&& other) noexcept;

    void send(std::span<const std::byte> data);

    // timeout.count() <= 0 blocks indefinitely; > 0 applies SO_RCVTIMEO
    // and returns 0 on timeout instead of throwing.
    std::size_t receive(std::span<std::byte> buffer,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds{-1});

private:
    SOCKET socket_ = INVALID_SOCKET;
    sockaddr_in group_addr_{};
    std::shared_ptr<detail::WinsockContext> wsa_;
};

} // namespace ddslite::transport
