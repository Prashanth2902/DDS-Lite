#include <ddslite/transport/udp_multicast_transport.hpp>

#include <mutex>
#include <string>
#include <utility>

namespace ddslite::transport {

namespace {

std::string format_wsa_error(int code) {
    char* message_buffer = nullptr;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&message_buffer), 0, nullptr);

    std::string message = (size > 0 && message_buffer) ? std::string(message_buffer, size) : "Unknown error";
    if (message_buffer) {
        LocalFree(message_buffer);
    }
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }
    return message;
}

} // namespace

transport_error::transport_error(std::string_view what, int wsa_error_code)
    : std::runtime_error(std::string(what) + ": " + format_wsa_error(wsa_error_code) +
                          " (WSA error " + std::to_string(wsa_error_code) + ")"),
      wsa_error_code_(wsa_error_code) {}

namespace detail {

// Pairs WSAStartup/WSACleanup 1:1, refcounted process-wide via the
// weak_ptr below, so the public API never exposes Winsock lifecycle
// calls and can't be misused.
class WinsockContext {
public:
    WinsockContext() {
        WSADATA data;
        int result = WSAStartup(MAKEWORD(2, 2), &data);
        if (result != 0) {
            throw transport_error("WSAStartup failed", result);
        }
    }

    ~WinsockContext() { WSACleanup(); }

    WinsockContext(const WinsockContext&) = delete;
    WinsockContext& operator=(const WinsockContext&) = delete;
};

std::shared_ptr<WinsockContext> acquire_winsock_context() {
    static std::mutex mutex;
    static std::weak_ptr<WinsockContext> instance;

    std::lock_guard<std::mutex> lock(mutex);
    if (auto existing = instance.lock()) {
        return existing;
    }
    auto created = std::make_shared<WinsockContext>();
    instance = created;
    return created;
}

} // namespace detail

UdpMulticastTransport::UdpMulticastTransport(std::string_view multicast_group_ip,
                                              std::uint16_t port,
                                              std::string_view local_interface_ip)
    : wsa_(detail::acquire_winsock_context()) {
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) {
        throw transport_error("socket() failed", WSAGetLastError());
    }

    BOOL reuse = TRUE;
    if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                    sizeof(reuse)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        closesocket(socket_);
        throw transport_error("setsockopt(SO_REUSEADDR) failed", error);
    }

    // Windows-specific: bind to INADDR_ANY, not the multicast group
    // address (unlike the common Linux convention).
    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = htons(port);
    if (bind(socket_, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        closesocket(socket_);
        throw transport_error("bind() failed", error);
    }

    ip_mreq membership{};
    if (inet_pton(AF_INET, std::string(multicast_group_ip).c_str(), &membership.imr_multiaddr) != 1) {
        closesocket(socket_);
        throw transport_error("invalid multicast group address", WSAGetLastError());
    }
    if (inet_pton(AF_INET, std::string(local_interface_ip).c_str(), &membership.imr_interface) != 1) {
        closesocket(socket_);
        throw transport_error("invalid local interface address", WSAGetLastError());
    }
    if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&membership),
                    sizeof(membership)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        closesocket(socket_);
        throw transport_error("setsockopt(IP_ADD_MEMBERSHIP) failed", error);
    }

    // Explicit rather than relying on Windows' default so two demo
    // processes on the same host can see each other's traffic.
    BOOL loopback = TRUE;
    if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loopback),
                    sizeof(loopback)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        closesocket(socket_);
        throw transport_error("setsockopt(IP_MULTICAST_LOOP) failed", error);
    }

    group_addr_ = sockaddr_in{};
    group_addr_.sin_family = AF_INET;
    group_addr_.sin_port = htons(port);
    group_addr_.sin_addr = membership.imr_multiaddr;
}

UdpMulticastTransport::~UdpMulticastTransport() {
    if (socket_ != INVALID_SOCKET) {
        ip_mreq membership{};
        membership.imr_multiaddr = group_addr_.sin_addr;
        membership.imr_interface.s_addr = INADDR_ANY;
        setsockopt(socket_, IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<const char*>(&membership),
                   sizeof(membership));
        closesocket(socket_);
    }
}

UdpMulticastTransport::UdpMulticastTransport(UdpMulticastTransport&& other) noexcept
    : socket_(std::exchange(other.socket_, INVALID_SOCKET)),
      group_addr_(other.group_addr_),
      wsa_(std::move(other.wsa_)) {}

UdpMulticastTransport& UdpMulticastTransport::operator=(UdpMulticastTransport&& other) noexcept {
    if (this != &other) {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
        }
        socket_ = std::exchange(other.socket_, INVALID_SOCKET);
        group_addr_ = other.group_addr_;
        wsa_ = std::move(other.wsa_);
    }
    return *this;
}

void UdpMulticastTransport::send(std::span<const std::byte> data) {
    int sent = sendto(socket_, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()),
                       0, reinterpret_cast<const sockaddr*>(&group_addr_), sizeof(group_addr_));
    if (sent == SOCKET_ERROR) {
        throw transport_error("sendto() failed", WSAGetLastError());
    }
}

std::size_t UdpMulticastTransport::receive(std::span<std::byte> buffer,
                                            std::chrono::milliseconds timeout) {
    // SO_RCVTIMEO takes a DWORD milliseconds value on Windows, not a
    // struct timeval like POSIX. 0 means block indefinitely.
    DWORD timeout_ms = timeout.count() > 0 ? static_cast<DWORD>(timeout.count()) : 0;
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms),
                    sizeof(timeout_ms)) == SOCKET_ERROR) {
        throw transport_error("setsockopt(SO_RCVTIMEO) failed", WSAGetLastError());
    }

    int received = recvfrom(socket_, reinterpret_cast<char*>(buffer.data()),
                             static_cast<int>(buffer.size()), 0, nullptr, nullptr);
    if (received == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT) {
            return 0;
        }
        throw transport_error("recvfrom() failed", error);
    }
    return static_cast<std::size_t>(received);
}

} // namespace ddslite::transport
