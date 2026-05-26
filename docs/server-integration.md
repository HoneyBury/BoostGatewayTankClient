# 服务端联调说明

完整客户端部署、运行、打包和常见问题见
[客户端部署与使用说明](client-deployment.md)。服务端部署快速入口见服务端仓库的
`docs/deployment-quickstart.md`。

客户端第三方资产、SDK package 与新机器依赖恢复说明见
[third-party-assets.md](third-party-assets.md)。

## 本地目录约定

推荐服务端和客户端仓库放在同一级目录：

```text
/Users/honeybury/workspace/BoostAsioDemo
/Users/honeybury/workspace/BoostGatewayTankClient
```

客户端构建：

```bash
cmake -S . -B build/local \
  -DBOOST_GATEWAY_SERVER_ROOT=/Users/honeybury/workspace/BoostAsioDemo \
  -DBOOST_GATEWAY_SERVER_BUILD_DIR=/Users/honeybury/workspace/BoostAsioDemo/build
cmake --build build/local
ctest --test-dir build/local --output-on-failure
```

客户端运行：

```bash
BGTC_GATEWAY_HOST=127.0.0.1 \
BGTC_GATEWAY_PORT=9201 \
./build/local/boost_gateway_tank_client
```

如果服务端使用 OrbStack / Docker Compose：

```bash
cd /Users/honeybury/workspace/BoostAsioDemo
docker compose -f env/docker/docker-compose.yml up -d
curl http://127.0.0.1:9080/health
```

客户端连接 `127.0.0.1:9201`。

## 两个仓库的职责

服务端仓库负责：

- C++ SDK 实现、package config、版本兼容矩阵。
- Gateway、login、room、battle、matchmaking、leaderboard、replay 服务。
- 坦克 demo server 和 authoritative simulation。
- 服务端性能、稳定性、生产证据和 validation summary。

客户端仓库负责：

- Qt 应用、窗口、渲染、输入和桌面交互。
- 坦克协议 adapter 和客户端模型。
- 客户端诊断、日志、回放 UI、排行榜 UI。
- 桌面端素材、打包、发布和安装说明。

共享契约：

- SDK 版本。
- Gateway 版本。
- Tank protocol 版本。
- Push payload 结构。
- 错误码和错误语义。
- 联调脚本和验证摘要格式。

## SDK 消费方式

优先级从高到低：

1. 使用服务端发布的 SDK install prefix 或 release artifact。
2. 使用服务端 build 目录中的完整 CMake package config。
3. 开发期 fallback 到服务端已有 `libboost_gateway_sdk.a` 和 SDK headers。

客户端不复制服务端源码，不把服务端 CMake 子工程作为默认依赖。

开发期建议显式传入 `BOOST_GATEWAY_SERVER_BUILD_DIR`。live gate 会用该目录中的
`sdk/libboost_gateway_sdk.a` 重新配置客户端构建，确保 headless gate 与本次启动
的 gateway/backend 使用同一套服务端 build 产物。

## 联调门禁规划

当前客户端已经提供 headless gate：

```bash
./scripts/run-headless-gate.sh
```

默认读取：

```bash
BGTC_GATEWAY_HOST=127.0.0.1
BGTC_GATEWAY_PORT=9201
```

它会启动两个 SDK client，执行：

- connect
- login
- create room
- join room
- ready
- start battle
- send battle input
- finish battle
- observe battle push 或 response 证据
- leaderboard top

执行后会在当前目录写出：

```text
tank-client-headless-summary.json
```

也可以直接运行完整 live gate，让脚本自动构建并启动本地服务端栈：

```bash
./scripts/run-live-gate.sh \
  --server-root /Users/honeybury/workspace/BoostAsioDemo \
  --server-build-dir /Users/honeybury/workspace/BoostAsioDemo/build
```

输出摘要：

```text
runtime/validation/tank-client-live-gate-summary.json
runtime/validation/tank-client-headless-summary.json
```

客户端本地验证还包含离线 Qt UI smoke gate：

```bash
./scripts/verify-client-local.sh
```

该脚本会构建并运行 `tank_protocol_smoke_test` 和 `tank_ui_smoke_test`。UI gate 使用
`QT_QPA_PLATFORM=offscreen`，只验证核心 widgets 构造、战斗 snapshot/settlement
渲染路径和基础页面稳定性，不触发真实 gateway 网络请求。

长期联调门禁还应该继续扩展：

- 启动本地服务端栈。
- 启动两个客户端 actor，或一个 UI 加一个 headless bot。
- 两个不同用户完成登录。
- 创建房间、加入房间、双方 ready。
- 开始 battle。
- 两端收到递增 snapshot frame。
- 本地输入能改变服务端 authoritative snapshot。
- 临时断线后能重连并恢复 snapshot。
- finish / settlement 能写入排行榜。
- 回放可查询并能被客户端渲染。

当前自动化已经包含 headless bot、Qt offscreen UI smoke、Qt UI flow 和真实 live gate。
live gate 会自动启动服务端栈，并覆盖注册、三客户端登录、建房、加入、踢人、
转让房主、ready、start、房间列表、房间详情、战斗输入、battle state 查询恢复、
真断线重连恢复、finish、回放加载与 leaderboard。UI flow gate 覆盖本地 profile
隔离、回放 timeline 解析/渲染入口和核心 Qt 页面构造。

## 联调风险

- 当前客户端已能消费 SDK 和真实 battle push；房间列表、房间详情、房主管理、
  battle state 查询恢复和回放加载已通过 SDK 和 live gate 验证。
- 回放播放器当前基于服务端 replay frames JSON，后续如果服务端回放格式升级，
  需要保持 `tank.replay_frame` adapter 的兼容层。
- 道具最小协议已固定为 battle input/snapshot adapter：客户端发送 `pickup:<item_id>`，
  服务端 snapshot 返回 `items/events/buffs`，当前 live gate 会验证 speed 道具拾取与
  buff 生效。更复杂的道具规则仍需后续单独扩展。

## 下一轮跨仓联调建议

1. 服务端继续扩展道具类型、冷却、持续时间衰减和结算影响。
2. 客户端补更细的道具 HUD、拾取动画、按键设置和日志导出。
3. 两仓共同产出包含多道具、多局回放播放和打包 smoke 的 `runtime/validation/tank-client-integration-summary.json`。
