# 客户端部署与使用说明

本文档说明如何构建、运行和打包 BoostGateway Tank Client，并连接到服务端
gateway。客户端仓库不包含服务端源码，只消费服务端产出的 C++ SDK。

## 前置条件

- CMake 3.24+
- C++20 编译器
- Qt 6 或 Qt 5 Widgets
- 同级目录中存在服务端仓库，推荐：

```text
/Users/honeybury/workspace/BoostAsioDemo
/Users/honeybury/workspace/BoostGatewayTankClient
```

如果已经有独立安装的 SDK，也可以用 `BOOST_GATEWAY_SDK_DIR` 指向 SDK package。

## 构建客户端

开发期推荐直接使用服务端 build 目录中的 SDK：

```bash
cd /Users/honeybury/workspace/BoostGatewayTankClient

cmake -S . -B build/local \
  -DBOOST_GATEWAY_SERVER_ROOT=/Users/honeybury/workspace/BoostAsioDemo \
  -DBOOST_GATEWAY_SERVER_BUILD_DIR=/Users/honeybury/workspace/BoostAsioDemo/build

cmake --build build/local
```

运行本地测试：

```bash
./scripts/verify-client-local.sh
```

该脚本会构建并运行：

- `tank_protocol_smoke_test`
- `tank_ui_smoke_test`
- `tank_ui_flow_test`
- `tank_headless_gate` 构建目标

## 启动服务端

### 推荐：OrbStack / Docker Compose

```bash
cd /Users/honeybury/workspace/BoostAsioDemo

docker compose -f env/docker/docker-compose.yml build
docker compose -f env/docker/docker-compose.yml up -d
```

检查：

```bash
docker compose -f env/docker/docker-compose.yml ps
curl http://127.0.0.1:9080/health
curl http://127.0.0.1:9080/ready
```

客户端连接地址：

```text
host = 127.0.0.1
port = 9201
```

停止：

```bash
docker compose -f env/docker/docker-compose.yml down
```

### 开发联调：脚本自动启动服务端

如果只是验证客户端和服务端是否兼容，直接运行：

```bash
cd /Users/honeybury/workspace/BoostGatewayTankClient

./scripts/run-live-gate.sh \
  --server-root /Users/honeybury/workspace/BoostAsioDemo \
  --server-build-dir /Users/honeybury/workspace/BoostAsioDemo/build
```

该脚本会动态分配端口，启动 login、room、battle、matchmaking、leaderboard 和
gateway，然后运行 headless 客户端验证完整业务闭环。

## 运行桌面客户端

服务端启动后：

```bash
cd /Users/honeybury/workspace/BoostGatewayTankClient

BGTC_GATEWAY_HOST=127.0.0.1 \
BGTC_GATEWAY_PORT=9201 \
./build/local/boost_gateway_tank_client
```

登录窗口支持：

- 注册账号
- 登录
- host / port 编辑
- user id / token 编辑

当前开发凭证建议使用：

```text
user_id = alice
token = token:alice
```

应用会保存本地 profile，包括 gateway host/port、默认房间、玩家前缀和 token。
测试环境可以通过 `BGTC_PROFILE_PATH=/tmp/profile.ini` 隔离 profile 文件。

## 客户端主要操作

大厅：

- 创建房间
- 加入房间
- 准备 / 取消准备
- 开始战斗
- 踢出成员
- 转让房主
- 查询房间列表 / 房间详情 / 排行榜

战斗：

- `WASD` / 方向键：移动
- 空格：攻击当前 snapshot 中的第一个对手
- `E`：拾取当前 snapshot 中的第一个道具
- `F`：结束战斗

回放：

- 加载当前 battle replay
- 播放 / 暂停
- 上一帧 / 下一帧
- 0.5x / 1x / 2x / 4x 倍速

## 打包客户端

```bash
cd /Users/honeybury/workspace/BoostGatewayTankClient
./scripts/package-client.sh
```

产物默认输出到：

```text
build/package/
```

macOS 当前生成：

```text
BoostGatewayTankClient-0.1.0-Darwin.tar.gz
```

当前包定位为开发/联调包，仍要求目标机器具备对应 Qt runtime。后续原生安装器、
Qt runtime 收集、签名和日志导出继续在发布专项中完善。

## 联调验证

对已经启动的 gateway 执行 headless gate：

```bash
BGTC_GATEWAY_HOST=127.0.0.1 \
BGTC_GATEWAY_PORT=9201 \
./scripts/run-headless-gate.sh
```

自动启动服务端并验证：

```bash
./scripts/run-live-gate.sh \
  --server-root /Users/honeybury/workspace/BoostAsioDemo \
  --server-build-dir /Users/honeybury/workspace/BoostAsioDemo/build
```

当前 live gate 覆盖：

- 注册 / 登录
- 三客户端连接
- 创建房间 / 加入 / 踢人 / 转让房主
- ready / start battle
- 移动输入
- 道具生成 / 拾取 / speed buff
- battle state 查询
- 断线重连后恢复 snapshot
- finish / leaderboard
- replay load

## 常见问题

- `9201` 是 gateway TCP 协议端口，不是浏览器页面。
- 如果客户端提示连接失败，先确认服务端 gateway 已监听 `127.0.0.1:9201`。
- 如果 SDK API 不匹配，重新配置客户端并显式传入 `BOOST_GATEWAY_SERVER_BUILD_DIR`。
- 如果 Qt 页面无法启动，先运行 `./scripts/verify-client-local.sh` 区分 Qt 环境问题和服务端问题。
