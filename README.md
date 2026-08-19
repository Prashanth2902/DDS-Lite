# DDS Lite

A minimal C++ pub-sub middleware implementing DDS-style discovery, typed messaging, and QoS — benchmarked against ROS2.

## Goal

DDS Lite builds a lightweight publish-subscribe messaging system from scratch that mimics what ROS2/DDS does under the hood — topic discovery, typed message passing, QoS policies, and serialization — then benchmarks it against real ROS2 to demonstrate understanding of the underlying tradeoffs. It's a simulation-only project (no physical hardware) scoped for a few weekends of focused work.

## Architecture

- **Transport** — UDP multicast as the wire protocol between nodes, mirroring the layer DDS/ROS2 actually uses for discovery and data exchange.
- **Discovery** — Nodes broadcast "I publish topic X" / "I subscribe to topic X" on a well-known multicast group, so publishers and subscribers find each other without a central broker.
- **Serialization** — Protobuf-defined messages so topics are strongly typed.
- **QoS policies** — Best-effort (fire and forget, like sensor streams) and Reliable (ack + retransmit, like control commands), with optional history-depth buffering for late-joining subscribers.
- **Node API** — A small C++ library so a "user" node just does:
  ```cpp
  Node node("camera_node");
  auto pub = node.advertise<ImageMsg>("camera/image", QoS::BestEffort);
  pub.publish(msg);
  ```
- **Benchmark harness** — Measures latency (p50/p99) and throughput at increasing message rates/sizes, and compares directly against the same workload run over real ROS2.

## Status

Early scaffolding — CMake build graph in place, core transport/discovery/QoS implementation in progress. See the roadmap below for current progress.

## Building

```bash
cmake -B build
cmake --build build
```

No external dependencies are required yet. Protobuf, GoogleTest, and other tooling will be added as the corresponding pieces (serialization, tests) are implemented.

## Roadmap

- [ ] Working transport + discovery layer
- [ ] Typed pub/sub API with Protobuf messages
- [ ] Best-effort and Reliable QoS implemented
- [ ] Unit tests for core logic
- [ ] CI pipeline
- [ ] Benchmark harness + ROS2 comparison results (latency/throughput graphs)
- [ ] Architecture diagram and benchmark results in this README
- [ ] (Stretch) ROS2 bridge node
- [ ] (Stretch) Fault injection / reliable-QoS recovery demo

## License

See [LICENSE](LICENSE).
