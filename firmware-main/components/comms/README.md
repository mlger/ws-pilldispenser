# comms (Phase 4)

MQTT/TLS client: offline queue, LWT, retained state, reconnect backoff.
Under QEMU it rides OpenCores Ethernet (`CONFIG_ETH_USE_OPENETH`) with the
broker at 10.0.2.2 (host); on hardware it rides Wi-Fi via `hal_net`.
