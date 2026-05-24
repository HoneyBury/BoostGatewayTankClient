# BoostGateway Tank Client

这是一个基于 Qt/C++ 的多人在线同屏坦克大战验证客户端，用于验证
BoostGateway 实时服务框架、C++ SDK、房间大厅、战斗同步、排行榜、回放
等完整业务链路。

本仓库刻意与服务端仓库拆分。服务端仓库继续负责 gateway、SDK、实时运行
时、房间、战斗、排行榜等基础能力；客户端仓库只消费公开 SDK、协议文档
和联调约定，Qt UI、桌面端打包、素材和交互逻辑都留在本仓库内，避免把
Qt 依赖污染到服务端主工程。

## 项目目标

- 提供一个真实玩家可操作的多人同屏坦克大战验证客户端。
- 验证服务端 push snapshot 下的多人状态同步。
- 覆盖注册/登录、房间大厅、创建房间、加入房间、准备、开始战斗、输入、
  断线重连、结算、排行榜、回放和后续道具机制。
- 保持 SDK 通用性，不把 `tank_move()`、`tank_fire()` 这类业务接口写入
  公共 SDK。
- 为后续服务端/客户端联调提供清晰的版本、协议和验证边界。

## 目录结构

```text
cmake/        SDK 导入与 CMake 辅助脚本
docs/         架构、协议、联调和生产规划文档
src/app/      应用入口
src/core/     配置、会话状态和通用模型
src/sdk/      面向 Qt 的 SDK facade
src/tank/     坦克业务协议适配与客户端模型
src/ui/       Qt Widgets 窗口与渲染控件
tests/        客户端侧轻量 smoke test
scripts/      本地构建和运行脚本
assets/       后续美术、音效、地图和 Qt resource
```

## 构建

客户端使用 CMake + Qt Widgets。优先使用 Qt 6，也兼容 Qt 5。

推荐开发方式是在同级目录保留服务端仓库：

```text
/Users/honeybury/workspace/BoostAsioDemo
/Users/honeybury/workspace/BoostGatewayTankClient
```

构建命令：

```bash
cmake -S . -B build/local \
  -DBOOST_GATEWAY_SERVER_ROOT=/Users/honeybury/workspace/BoostAsioDemo \
  -DBOOST_GATEWAY_SERVER_BUILD_DIR=/Users/honeybury/workspace/BoostAsioDemo/build
cmake --build build/local
ctest --test-dir build/local --output-on-failure
```

也可以运行一键本地验证脚本：

```bash
./scripts/verify-client-local.sh
```

该脚本会同时运行协议 smoke test 和 Qt offscreen UI smoke gate。

如果 SDK 已经被单独安装或导出，可以直接指定 SDK 目录：

```bash
cmake -S . -B build/local \
  -DBOOST_GATEWAY_SDK_DIR=/path/to/boost-gateway-sdk
```

## 运行

先启动 BoostGateway 服务端栈，再运行客户端：

```bash
BGTC_GATEWAY_HOST=127.0.0.1 \
BGTC_GATEWAY_PORT=9201 \
./build/local/boost_gateway_tank_client
```

当前客户端状态：

- 登录窗口：支持 host、port、user id、token 输入。
- 大厅窗口：支持创建房间、加入房间、准备、开始战斗、离开房间、查询排行榜；
  房间列表和房间详情已通过 SDK 访问真实 gateway/backend，房主管理仍保留能力提示。
- 战斗窗口：支持网格渲染、服务端 snapshot 渲染、玩家 HP/Score/Pos HUD、
  结算卡片，WASD/方向键发送真实 `move:x,y`，空格发送 `attack:user_id`，
  F 发送 `finish:reason`。
- 排行榜窗口：支持格式化展示 top/rank，并在战斗结束后显示最近一局结算摘要。
- Headless gate：支持对运行中的 gateway 执行双客户端房间战斗闭环验证。

当前阶段目标是“可验证可联调的 MVP”，不是最终美术品质。

## 与服务端的关系

客户端不复制服务端源码。当前 SDK 导入策略如下：

- 优先通过 `boost_gateway_sdk` CMake package config 消费 SDK。
- 若服务端 build-tree package config 不完整，则开发期 fallback 到服务端已有
  `libboost_gateway_sdk.a` 和 SDK headers。
- 正式发布阶段应使用安装后的 SDK prefix 或 SDK release artifact。

详见 [docs/server-integration.md](docs/server-integration.md)。

## Headless 联调

服务端 gateway 启动后：

```bash
BGTC_GATEWAY_HOST=127.0.0.1 \
BGTC_GATEWAY_PORT=9201 \
./scripts/run-headless-gate.sh
```

该脚本会运行 `tank_headless_gate`，执行双客户端 login、房间、ready、battle、
input、finish、leaderboard 验证，并输出 `tank-client-headless-summary.json`。

如果希望脚本自动启动服务端 login、room、battle、match、leaderboard 和
gateway，并使用动态端口完成真实闭环验证，可以运行：

```bash
./scripts/run-live-gate.sh \
  --server-root /Users/honeybury/workspace/BoostAsioDemo \
  --server-build-dir /Users/honeybury/workspace/BoostAsioDemo/build
```

该脚本会在构建客户端前重新配置 `BOOST_GATEWAY_SERVER_BUILD_DIR`，确保
headless gate 链接到当前服务端 build 目录中的 SDK，避免客户端 SDK 与运行中
gateway 协议不一致。

## 下一步

整体路线以“真实多人同屏可玩”为主线。当前已接入 battle state 查询并可在重连后
主动恢复最新 snapshot；下一步优先推进完整 UI 自动化联调、真断线恢复场景、回放
和道具协议。详细规划见
[docs/production-roadmap.md](docs/production-roadmap.md)。
