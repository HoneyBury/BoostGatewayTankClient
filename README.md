# BoostGateway Tank Client

Qt/C++ multiplayer tank battle validation client for the BoostGateway realtime
server framework.

This repository is intentionally separate from the server repository.  The
client consumes the public C++ SDK and tank protocol contracts, while Qt,
desktop UI code, packaging, and assets stay here.

## Goals

- Provide a real player-facing validation client for the tank battle demo.
- Verify same-screen multiplayer state sync through server push snapshots.
- Exercise registration/login, room lobby, ready, battle, reconnect, replay,
  leaderboard, and future item/power-up flows.
- Keep game-specific UI and tank business adapters out of the server framework.

## Repository Layout

```text
cmake/        CMake helpers for importing BoostGateway SDK
docs/         architecture, integration, production planning
src/app/      application entry point
src/core/     config and session state
src/sdk/      Qt-friendly facade over boost_gateway::sdk::SdkClient
src/tank/     tank input/snapshot protocol adapter
src/ui/       Qt Widgets windows and rendering widgets
tests/        lightweight client-side smoke tests
scripts/      local build and integration helper scripts
assets/       future images, sounds, maps, and Qt resources
```

## Build

The client uses CMake and Qt Widgets.  Qt 6 is preferred; Qt 5 is accepted.

From this repository:

```bash
cmake -S . -B build \
  -DBOOST_GATEWAY_SERVER_ROOT=/Users/honeybury/workspace/BoostAsioDemo
cmake --build build
ctest --test-dir build --output-on-failure
```

If the SDK is installed or exported elsewhere, pass:

```bash
cmake -S . -B build -DBOOST_GATEWAY_SDK_DIR=/path/to/sdk/prefix/or/build/sdk
```

## Run

Start the BoostGateway server stack first, then run:

```bash
BGTC_GATEWAY_HOST=127.0.0.1 BGTC_GATEWAY_PORT=9201 ./build/boost_gateway_tank_client
```

Current UI status:

- Login window: host/port/user/token input.
- Lobby window: create room, join room, ready, start battle, leave room,
  leaderboard query.
- Battle window: grid renderer, tank/bullet snapshot rendering, WASD/arrows
  movement, space fire.

The first milestone is a playable verification MVP, not final game art.

## Compatibility

Current expected server-side boundary:

- BoostGateway SDK native major: `4.x`
- Gateway demo flow: login -> room -> ready -> battle -> push -> leaderboard
- Tank payloads: see [docs/protocol.md](docs/protocol.md)

The client should not include server framework source files.  Use SDK packages,
CMake package config, and protocol documents as the integration contract.
