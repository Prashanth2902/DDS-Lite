#include <ddslite/transport/udp_multicast_transport.hpp>

#include <chrono>
#include <iostream>
#include <span>
#include <string>
#include <thread>

int main() {
    try {
        ddslite::transport::UdpMulticastTransport transport("239.255.0.1", 30001);

        for (int i = 1; i <= 5; ++i) {
            std::string message = "hello-" + std::to_string(i);
            std::cout << "Sending: " << message << std::endl;

            std::span<const char> view(message.data(), message.size());
            transport.send(std::as_bytes(view));

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        std::cout << "Done sending." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
