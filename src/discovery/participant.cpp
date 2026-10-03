#include <ddslite/discovery/participant.hpp>

#include <algorithm>

namespace ddslite::discovery {

Participant::Participant(std::string node_name) : node_name_(std::move(node_name)) {}

void Participant::add_local_topic(std::string_view topic_name, Direction direction) {
    local_topics_.push_back(LocalTopic{std::string(topic_name), direction});
}

bool Participant::on_remote_announcement(std::string_view remote_node_name,
                                          std::string_view topic_name,
                                          Direction direction) {
    if (remote_node_name == node_name_) {
        return false;
    }

    auto it = std::find_if(remote_topics_.begin(), remote_topics_.end(), [&](const RemoteTopic& t) {
        return t.node_name == remote_node_name && t.topic_name == topic_name && t.direction == direction;
    });

    if (it != remote_topics_.end()) {
        it->last_seen = std::chrono::steady_clock::now();
        return false;
    }

    remote_topics_.push_back(RemoteTopic{std::string(remote_node_name), std::string(topic_name), direction,
                                          std::chrono::steady_clock::now()});
    return true;
}

} // namespace ddslite::discovery
