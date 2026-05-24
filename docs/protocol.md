# Tank Protocol Contract

This client keeps tank-specific business payloads outside the public SDK.  The
SDK sends opaque strings through generic battle APIs; this repository owns the
Qt-side adapter.

## Input

Sent with `SdkClient::send_battle_input()`.

```json
{
  "seq": 42,
  "actions": [
    {"type": "move", "dx": 0, "dy": -1},
    {"type": "fire", "direction": 0}
  ]
}
```

Supported actions:

| Action | Fields | Meaning |
| --- | --- | --- |
| `move` | `dx`, `dy` | Move one grid cell. No diagonal movement. |
| `fire` | `direction` | Fire in `0`, `90`, `180`, or `270` degrees. |
| `stop` | none | No movement for this tick. |

## Snapshot Push

Expected body, either directly or nested in a push envelope payload:

```json
{
  "frame": 150,
  "finished": false,
  "tanks": [
    {"user_id": "alice", "x": 5, "y": 3, "hp": 100, "direction": 0, "alive": true}
  ],
  "bullets": [
    {"id": 17, "x": 7, "y": 4, "dx": 0, "dy": -1, "owner": "alice"}
  ],
  "events": [
    {"type": "bullet_hit", "actor": "alice", "target": "bob", "damage": 25, "frame": 151}
  ]
}
```

## Compatibility Rules

- The client accepts extra fields for forward compatibility.
- The client treats invalid JSON or non-tank pushes as generic log messages.
- `seq` is client-generated and monotonically increasing per local player.
- The SDK remains generic; tank-specific APIs belong in this client adapter.
