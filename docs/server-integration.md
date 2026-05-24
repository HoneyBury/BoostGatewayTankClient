# Server Integration Plan

## Local Development

The server repository should be checked out next to this repository:

```text
/Users/honeybury/workspace/BoostAsioDemo
/Users/honeybury/workspace/BoostGatewayTankClient
```

Build this client against the server SDK:

```bash
cmake -S . -B build \
  -DBOOST_GATEWAY_SERVER_ROOT=/Users/honeybury/workspace/BoostAsioDemo
cmake --build build
```

Run against a local gateway:

```bash
BGTC_GATEWAY_HOST=127.0.0.1 BGTC_GATEWAY_PORT=9201 ./build/boost_gateway_tank_client
```

## Contract Between Repositories

Server repository owns:

- C++ SDK implementation and package config.
- Gateway, room, battle, matchmaking, leaderboard, replay services.
- Tank demo server and authoritative simulation.
- Compatibility matrix and validation summaries.

Client repository owns:

- Qt application shell and windows.
- Tank protocol adapter and rendering model.
- Input mapping, local state, diagnostics, replay UI.
- Desktop packaging and assets.

Shared contract:

- SDK version.
- Gateway version.
- Tank protocol version.
- Expected push payloads.
- Error code semantics.

## 联调门禁

The long-term integration gate should start the server stack, launch two client
actors or one UI plus one bot, and verify:

- both clients login with distinct users;
- room create/join/ready succeeds;
- battle starts and both clients receive increasing snapshot frames;
- local input changes authoritative snapshot state;
- disconnect/reconnect resumes state;
- finish/settlement updates leaderboard;
- replay is queryable and can be rendered.

The first automated version may use a headless Qt test or console bot before
full UI automation is added.
