#pragma once

// Demo-facing building block, not a committed public API yet — reachable
// from include/ddslite/ only because examples/ link ddslite PRIVATE and
// need direct access until a real Node API wraps this.

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace ddslite::discovery {

enum class Direction { Pub, Sub };

struct LocalTopic {
    std::string topic_name;
    Direction direction;
};

struct RemoteTopic {
    std::string node_name;
    std::string topic_name;
    Direction direction;
    std::chrono::steady_clock::time_point last_seen;
};

// Tracks a node's own advertised topics plus the remote topics it has
// heard announced by other nodes. Self-announcements (this node hearing
// its own multicast traffic) are filtered out here.
class Participant {
public:
    explicit Participant(std::string node_name);

    const std::string& node_name() const noexcept { return node_name_; }

    void add_local_topic(std::string_view topic_name, Direction direction);
    const std::vector<LocalTopic>& local_topics() const noexcept { return local_topics_; }

    // Returns true only the first time this (node, topic, direction)
    // tuple is seen; later sightings just refresh last_seen and return
    // false. Always returns false for remote_node_name == node_name().
    bool on_remote_announcement(std::string_view remote_node_name,
                                 std::string_view topic_name,
                                 Direction direction);

    const std::vector<RemoteTopic>& known_remote_topics() const noexcept { return remote_topics_; }

private:
    std::string node_name_;
    std::vector<LocalTopic> local_topics_;
    std::vector<RemoteTopic> remote_topics_;
};

} // namespace ddslite::discovery
