#include <ddslite/transport/udp_multicast_transport.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>

int main() {
    try {
        ddslite::transport::UdpMulticastTransport transport("239.255.0.1", 30001);

        std::cout << "Listening on 239.255.0.1:30001 ..." << std::endl;

        std::array<std::byte, 1024> buffer{};
        int messages_received = 0;
        int consecutive_timeouts = 0;

        while (messages_received < 5 && consecutive_timeouts < 3) {
            std::size_t received = transport.receive(buffer, std::chrono::milliseconds(5000));
            if (received == 0) {
                ++consecutive_timeouts;
                std::cout << "(timeout waiting for a message)" << std::endl;
                continue;
            }
            consecutive_timeouts = 0;
            ++messages_received;
            std::string text(reinterpret_cast<const char*>(buffer.data()), received);
            std::cout << "Received (" << received << " bytes): " << text << std::endl;
        }

        std::cout << "Done receiving." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
