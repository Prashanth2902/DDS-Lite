#pragma once

#include <ddslite/discovery/participant.hpp>
#include <ddslite/transport/udp_multicast_transport.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string_view>

namespace ddslite::discovery {

// Periodically announces a Participant's local topics over multicast and
// listens for other nodes' announcements, feeding them back into the
// Participant. Single-threaded: poll() interleaves sending and a
// short-timeout receive, since nothing else in the process currently
// needs the thread concurrently.
class DiscoveryService {
public:
    using DiscoveryCallback = std::function<void(const RemoteTopic&)>;

    DiscoveryService(Participant& participant,
                      std::string_view discovery_group_ip,
                      std::uint16_t discovery_port,
                      std::chrono::milliseconds announce_interval = std::chrono::milliseconds{1000},
                      std::string_view local_interface_ip = "0.0.0.0");

    // One tick: sends an announce burst if announce_interval has elapsed,
    // then attempts one bounded receive+parse.
    void poll();

    // Convenience for demos: calls poll() in a loop for total_duration.
    void run_for(std::chrono::milliseconds total_duration);

    // Invoked synchronously from poll()/run_for() whenever a remote topic
    // is newly seen (Participant::on_remote_announcement returns true).
    void set_on_remote_topic_discovered(DiscoveryCallback callback);

private:
    Participant& participant_;
    ddslite::transport::UdpMulticastTransport transport_;
    std::chrono::milliseconds announce_interval_;
    std::chrono::steady_clock::time_point last_announce_time_{};
    DiscoveryCallback on_discovered_;

    void send_announcements();
    void receive_and_parse(std::chrono::milliseconds receive_timeout);
};

} // namespace ddslite::discovery
