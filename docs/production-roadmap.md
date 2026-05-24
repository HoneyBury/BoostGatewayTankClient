# Production Roadmap

The direction is a multiplayer same-screen online tank battle client, backed by
BoostGateway services and the public C++ SDK.

## P0: Client Skeleton and SDK Boundary

- Qt/CMake project builds independently.
- SDK imported through CMake package config.
- Login, lobby, and battle windows exist.
- Tank input/snapshot adapter has smoke tests.
- Documentation defines server/client boundary.

Exit criteria: client builds without copying server source code.

## P1: Real Login and Lobby

- Registration/login window with saved local profiles.
- Gateway connection diagnostics and SDK version display.
- Room list, create, join, leave, ready, owner transfer, kick.
- Clear error messages for backend unavailable, timeout, duplicate room, full room.

Exit criteria: two desktop clients can enter the same room reliably.

## P2: Online Battle MVP

- Start battle from room.
- Receive authoritative tank snapshots.
- Render tanks, bullets, hit events, HP, score, and finish state.
- Keyboard input sends `tank.input` with local seq.
- Basic latency/frame lag display.

Exit criteria: two clients see the same battle state and can move/fire.

## P3: Reconnect, Resume, and Stability

- Heartbeat and disconnect UI.
- Reconnect helper with retry/backoff.
- Resume snapshot application after reconnect.
- Client-side event queue prevents UI thread and SDK callback interference.

Exit criteria: temporary disconnect does not lose battle state within the server resume window.

## P4: Replay and Leaderboard

- Leaderboard top/rank window.
- Settlement detail page.
- Replay list/query integration.
- Replay playback timeline, pause, seek, speed control.

Exit criteria: a finished battle can be reviewed and its result appears in leaderboard.

## P5: Items and Gameplay Presentation

- Item/power-up protocol adapter.
- Item spawn/rendering, pickup events, cooldowns, buff state.
- Better tank sprites, tile maps, effects, and sound.
- Optional spectator mode.

Exit criteria: gameplay has visible item mechanics without changing SDK generic APIs.

## P6: Packaging and Release

- macOS/Windows packaging.
- Runtime SDK library discovery and diagnostics.
- Signed/notarized release path where appropriate.
- Version matrix: client, SDK, gateway, tank protocol.

Exit criteria: non-developer users can install and connect to a deployed test server.

## P7: Production Observability

- Local logs with player id, room id, battle id, request step, error code.
- In-client diagnostics panel for ping, frame, push rate, reconnect count.
- Exportable bug report bundle.
- Automated UI/headless integration gate.

Exit criteria: client issues can be triaged from logs and reproduced through automated gates.
