#include <ddslite/discovery/discovery_service.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace ddslite::discovery {

namespace {

// Throwaway, pre-Protobuf wire format: "ANNOUNCE|<node>|<PUB|SUB>|<topic>".
// One datagram per record. Replaced wholesale once Protobuf is introduced.
constexpr std::string_view kAnnounceTag = "ANNOUNCE";
constexpr char kDelimiter = '|';

std::string format_announcement(const Participant& participant, const LocalTopic& topic) {
    std::string direction_token = topic.direction == Direction::Pub ? "PUB" : "SUB";
    return std::string(kAnnounceTag) + kDelimiter + participant.node_name() + kDelimiter +
           direction_token + kDelimiter + topic.topic_name;
}

std::vector<std::string_view> split(std::string_view text, char delimiter) {
    std::vector<std::string_view> tokens;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t pos = text.find(delimiter, start);
        if (pos == std::string_view::npos) {
            tokens.push_back(text.substr(start));
            break;
        }
        tokens.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    return tokens;
}

struct ParsedAnnouncement {
    std::string_view node_name;
    Direction direction;
    std::string_view topic_name;
};

// Returns std::nullopt for anything that isn't a well-formed announcement
// (malformed or foreign traffic on the discovery channel is silently
// discarded, not an error).
std::optional<ParsedAnnouncement> parse_announcement(std::string_view datagram) {
    auto tokens = split(datagram, kDelimiter);
    if (tokens.size() != 4 || tokens[0] != kAnnounceTag) {
        return std::nullopt;
    }
    if (tokens[2] == "PUB") {
        return ParsedAnnouncement{tokens[1], Direction::Pub, tokens[3]};
    }
    if (tokens[2] == "SUB") {
        return ParsedAnnouncement{tokens[1], Direction::Sub, tokens[3]};
    }
    return std::nullopt;
}

} // namespace

DiscoveryService::DiscoveryService(Participant& participant,
                                    std::string_view discovery_group_ip,
                                    std::uint16_t discovery_port,
                                    std::chrono::milliseconds announce_interval,
                                    std::string_view local_interface_ip)
    : participant_(participant),
      transport_(discovery_group_ip, discovery_port, local_interface_ip),
      announce_interval_(announce_interval) {}

void DiscoveryService::send_announcements() {
    for (const auto& topic : participant_.local_topics()) {
        std::string message = format_announcement(participant_, topic);
        std::span<const char> view(message.data(), message.size());
        transport_.send(std::as_bytes(view));
    }
}

void DiscoveryService::receive_and_parse(std::chrono::milliseconds receive_timeout) {
    std::array<std::byte, 512> buffer{};
    std::size_t received = transport_.receive(buffer, receive_timeout);
    if (received == 0) {
        return;
    }

    std::string_view datagram(reinterpret_cast<const char*>(buffer.data()), received);
    auto parsed = parse_announcement(datagram);
    if (!parsed) {
        return;
    }

    bool newly_seen =
        participant_.on_remote_announcement(parsed->node_name, parsed->topic_name, parsed->direction);
    if (newly_seen && on_discovered_) {
        on_discovered_(participant_.known_remote_topics().back());
    }
}

void DiscoveryService::poll() {
    auto now = std::chrono::steady_clock::now();
    if (now - last_announce_time_ >= announce_interval_) {
        send_announcements();
        last_announce_time_ = now;
    }
    receive_and_parse(std::chrono::milliseconds{200});
}

void DiscoveryService::run_for(std::chrono::milliseconds total_duration) {
    auto deadline = std::chrono::steady_clock::now() + total_duration;
    while (std::chrono::steady_clock::now() < deadline) {
        poll();
    }
}

void DiscoveryService::set_on_remote_topic_discovered(DiscoveryCallback callback) {
    on_discovered_ = std::move(callback);
}

} // namespace ddslite::discovery
