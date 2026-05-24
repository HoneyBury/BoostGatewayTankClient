# 坦克大战协议说明

客户端保持 SDK 通用性。SDK 只发送/接收通用 battle 字符串 payload，坦克业务
语义由本仓库的 `src/tank/` 适配层负责。

## 输入协议

客户端通过 `SdkClient::send_battle_input()` 发送坦克输入。

```json
{
  "seq": 42,
  "actions": [
    {"type": "move", "dx": 0, "dy": -1},
    {"type": "fire", "direction": 0}
  ]
}
```

支持的 action：

| action | 字段 | 含义 |
| --- | --- | --- |
| `move` | `dx`, `dy` | 移动一个格子，不允许斜向移动。 |
| `fire` | `direction` | 向 `0`、`90`、`180`、`270` 方向开火。 |
| `stop` | 无 | 当前 tick 不移动。 |

客户端规则：

- `seq` 由客户端生成，单个玩家会话内单调递增。
- 本地按键只生成输入，最终位置以服务端 authoritative snapshot 为准。
- 非法输入不应直接修改本地权威状态。

## Snapshot Push

服务端 push 的 `tank.snapshot` 可以直接作为 body，也可以嵌套在 push envelope 的
`payload` 字段中。客户端需要兼容两种形态。

```json
{
  "frame": 150,
  "finished": false,
  "tanks": [
    {
      "user_id": "alice",
      "x": 5,
      "y": 3,
      "hp": 100,
      "direction": 0,
      "alive": true
    }
  ],
  "bullets": [
    {"id": 17, "x": 7, "y": 4, "dx": 0, "dy": -1, "owner": "alice"}
  ],
  "events": [
    {
      "type": "bullet_hit",
      "actor": "alice",
      "target": "bob",
      "damage": 25,
      "frame": 151
    }
  ]
}
```

## 当前服务端 Battle State Push

当前 BoostGateway 主链已存在的 battle push 是 key-value 字符串格式，例如：

```text
battle_state:kind=started:room_id=room_1:battle_id=battle_1
battle_state:kind=frame:room_id=room_1:battle_id=battle_1:frame=12:trigger=tick
battle_state:kind=settlement:room_id=room_1:battle_id=battle_1:reason=surrender:user_id=alice
battle_state:kind=finished:room_id=room_1:battle_id=battle_1:reason=surrender:user_id=alice
```

客户端当前同时支持：

- 解析现有 `battle_state:*` push，用于真实联调 gate。
- 解析未来 `tank.snapshot` JSON，用于同屏坦克渲染。

这意味着 P2 的联调可以先基于现有主链完成，随后服务端补齐更丰富的
`tank.snapshot` 后，客户端战斗画面会自然升级到完整同屏渲染。

## 结算协议

战斗结束后，服务端应产生 settlement，并写入排行榜。客户端后续需要展示：

- battle id / room id
- 胜利玩家
- 每个玩家的击杀、死亡、伤害、分数
- 结束原因
- 总帧数

## 兼容策略

- 客户端允许服务端 snapshot 增加额外字段。
- 客户端遇到非坦克 push 时只作为普通日志展示。
- JSON 无法解析时不崩溃，只记录诊断信息。
- 坦克业务 API 不进入公共 SDK，只存在客户端 adapter 中。

## 后续协议扩展

- `tank.item_spawn`：道具生成。
- `tank.item_pickup`：玩家拾取道具。
- `tank.buff_update`：增益状态变化。
- `tank.replay_frame`：回放帧数据。
- `tank.spectator_snapshot`：观战视角 snapshot。
