#include <ddslite/discovery/discovery_service.hpp>
#include <ddslite/discovery/participant.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>

using ddslite::discovery::Direction;
using ddslite::discovery::DiscoveryService;
using ddslite::discovery::Participant;
using ddslite::discovery::RemoteTopic;

namespace {

bool is_complementary_match(const Participant& participant, const RemoteTopic& remote) {
    Direction complementary = remote.direction == Direction::Pub ? Direction::Sub : Direction::Pub;
    const auto& locals = participant.local_topics();
    return std::any_of(locals.begin(), locals.end(), [&](const auto& local) {
        return local.topic_name == remote.topic_name && local.direction == complementary;
    });
}

} // namespace

int main() {
    try {
        Participant participant("node_b");
        participant.add_local_topic("camera/image", Direction::Sub);

        DiscoveryService service(participant, "239.255.0.2", 30000);
        service.set_on_remote_topic_discovered([&](const RemoteTopic& remote) {
            std::cout << "[node_b] discovered node=" << remote.node_name
                      << " topic=" << remote.topic_name
                      << " direction=" << (remote.direction == Direction::Pub ? "PUB" : "SUB");
            if (is_complementary_match(participant, remote)) {
                std::cout << "  <-- MATCH (publisher/subscriber pair)";
            }
            std::cout << std::endl;
        });

        service.run_for(std::chrono::milliseconds{15000});

        std::cout << "[node_b] final known remote topics:" << std::endl;
        for (const auto& remote : participant.known_remote_topics()) {
            std::cout << "  node=" << remote.node_name << " topic=" << remote.topic_name
                      << " direction=" << (remote.direction == Direction::Pub ? "PUB" : "SUB") << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
