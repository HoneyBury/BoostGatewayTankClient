# Client Architecture

## Boundary

The client is a Qt/C++ product repository.  It depends on the public
BoostGateway C++ SDK and tank protocol JSON.  It must not include gateway,
room, battle, leaderboard, or server framework source code.

## Layers

```text
Qt UI
  LoginWindow / LobbyWidget / BattleWidget
        |
Application state
  AppConfig / ClientSession
        |
Client facade
  GatewayClient
        |
Business protocol adapter
  TankProtocol / TankWorldModel
        |
BoostGateway C++ SDK
  boost_gateway::sdk::SdkClient
        |
Gateway server
```

## Threading

SDK callbacks may arrive away from the Qt UI flow.  `GatewayClient` forwards
push and disconnect events into Qt using queued invocation before UI widgets
consume them.  UI code must not directly block inside SDK callbacks.

## SDK Import Strategy

Preferred development mode:

```bash
cmake -S . -B build \
  -DBOOST_GATEWAY_SERVER_ROOT=/Users/honeybury/workspace/BoostAsioDemo
```

Release mode should use an installed SDK prefix or package artifact:

```bash
cmake -S . -B build -DBOOST_GATEWAY_SDK_DIR=/opt/boost-gateway-sdk
```

The server repository remains the source of truth for SDK implementation,
compatibility matrix, and protocol behavior.

## Future Modules

- `src/replay/`: replay loading, timeline, seek, pause/resume playback.
- `src/items/`: item/power-up model, icons, cooldown rendering.
- `src/diagnostics/`: latency, frame lag, SDK version, reconnect attempts.
- `src/bot/`: optional local headless bot for two-player validation.
- `assets/`: sprite sheets, tiles, sounds, map resources.
