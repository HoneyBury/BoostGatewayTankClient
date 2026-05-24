# 服务端联调说明

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

第一版自动化可以先用 headless bot 或 Qt 非 UI 测试完成，不必一开始就做完整
UI 自动化。

## 联调风险

- 当前客户端已能消费 SDK，但真实 snapshot push 还需要服务端 demo/gateway 路径
  明确落到客户端可接收的 push body。
- 房间列表、房间详情、踢人、转让房主等能力需要确认 SDK 是否已有公开 API；
  如果 SDK 没有，需要服务端先补通用 API，而不是客户端绕过 SDK 直连后端。
- 回放和道具需要服务端协议先固定版本，否则客户端只能做预留 UI。

## 下一轮跨仓联调建议

1. 服务端先提供一个固定端口或动态端口 summary，包含 gateway host/port。
2. 客户端读取该 summary 自动填充连接配置。
3. 服务端确保 battle snapshot push body 与 `docs/protocol.md` 兼容。
4. 客户端增加 headless bot，完成双人登录、房间、ready、battle、输入、snapshot 校验。
5. 两仓共同产出 `runtime/validation/tank-client-integration-summary.json`。
