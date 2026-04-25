# mqtt-test-cli

Small Rust CLI to validate the device MQTT flow.

## Build

```bash
cd tools/mqtt-test-cli
cargo build
```

## Subscribe to device debug topic

```bash
cargo run -- sub --topic psoc6/debug
```

## Publish a test message

```bash
cargo run -- pub --topic psoc6/debug --message '{"from":"host","ok":true}'
```

## Override broker

```bash
cargo run -- --host test.mosquitto.org --port 1883 sub --topic psoc6/debug
```
