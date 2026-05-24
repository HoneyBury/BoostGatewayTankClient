# 客户端架构说明

## 边界定位

本仓库是 Qt/C++ 桌面客户端仓库。它依赖 BoostGateway 公共 C++ SDK 和坦克
业务协议 JSON，但不直接包含 gateway、room、battle、leaderboard 或服务端
框架源码。

这样拆分的目标是：

- 服务端保持高性能实时框架定位。
- SDK 保持通用实时客户端能力，不掺入具体游戏规则。
- Qt、桌面 UI、素材、打包和交互逻辑独立演进。
- 客户端与服务端通过 SDK 版本、gateway 版本和协议版本联调。

## 分层结构

```text
Qt UI
  LoginWindow / LobbyWidget / BattleWidget
        |
应用状态
  AppConfig / ClientSession
        |
SDK facade
  GatewayClient
        |
坦克业务协议适配
  TankProtocol / TankWorldModel
        |
BoostGateway C++ SDK
  boost_gateway::sdk::SdkClient
        |
Gateway / 后端服务
```

## 核心模块

- `src/app/`：应用入口，初始化 Qt application 和登录窗口。
- `src/core/`：客户端配置、会话状态、连接状态枚举。
- `src/sdk/`：封装 `boost_gateway::sdk::SdkClient`，把 SDK 结果转换成 Qt 侧
  更容易使用的信号和方法。
- `src/tank/`：负责 `tank.input` 编码、`tank.snapshot` 解码和客户端战斗模型。
- `src/ui/`：登录、大厅、战斗画面等 Qt Widgets。
- 当前真实 SDK 功能入口覆盖注册/登录、房间列表/详情、踢出成员、转让房主、
  战斗状态查询、排行榜和回放加载；坦克业务输入与 snapshot 仍由客户端 adapter
  负责，不污染公共 SDK。

## 线程模型

SDK 的 push 和 disconnect callback 不应直接操作 UI。当前 `GatewayClient`
通过 Qt queued invocation 把事件转发到 Qt 线程，再由 UI 控件消费。

后续需要继续保持这个原则：

- SDK callback 中只做轻量解析和事件投递。
- UI 状态更新只在 Qt 主线程执行。
- 战斗 snapshot 可以进入客户端事件队列，避免 push 风暴阻塞 UI。
- 重连流程需要有明确状态机，避免 callback 中递归调用同一个 SDK client。

## SDK 导入策略

开发期推荐：

```bash
cmake -S . -B build/local \
  -DBOOST_GATEWAY_SERVER_ROOT=/Users/honeybury/workspace/BoostAsioDemo
```

正式或 CI 推荐：

```bash
cmake -S . -B build/local \
  -DBOOST_GATEWAY_SDK_DIR=/opt/boost-gateway-sdk
```

长期目标是由服务端仓库产出稳定 SDK package，客户端只消费 package，不依赖
服务端 build-tree 细节。

## 后续模块规划

- `src/replay/`：回放列表、时间轴、暂停/恢复、倍速播放；当前回放加载已先接入
  `GatewayClient::loadReplay()` 并在 UI 中展示服务端 JSON。
- `src/items/`：道具/增益模型、图标、冷却时间、拾取事件渲染。
- `src/diagnostics/`：延迟、frame lag、push rate、SDK version、重连次数。
- `src/bot/`：可选 headless bot，用于双人联调和自动化验证。
- `assets/`：坦克、地图 tile、子弹、爆炸、音效和 Qt resource。
