# 客户端打包与发布说明

本文档记录当前 Qt/C++ 客户端的最小发布路径。目标是先产出可归档、可交付、
可复现的开发包，后续再扩展为平台原生安装器。

## 本机打包

```bash
BOOST_GATEWAY_SERVER_ROOT=/Users/honeybury/workspace/BoostAsioDemo \
BOOST_GATEWAY_SERVER_BUILD_DIR=/Users/honeybury/workspace/BoostAsioDemo/build \
./scripts/package-client.sh
```

脚本会使用 Release 配置构建 `boost_gateway_tank_client`，再调用 CPack 生成包：

- macOS / Linux：默认 `TGZ`
- Windows：默认 `ZIP`

产物会输出在 `build/package/`。

## 包内容

- `bin/boost_gateway_tank_client`
- `docs/README.md` 等联调与路线文档

当前包仍依赖目标机器具备对应 Qt runtime 与 BoostGateway SDK 运行依赖。正式
面向非开发用户发布前，需要继续补：

- macOS `.app` bundle 与 Qt runtime 部署。
- Windows Qt runtime 收集和签名策略。
- 崩溃日志、运行日志和 profile 文件导出。

## 发布前验证

发布前至少执行：

```bash
./scripts/verify-client-local.sh
./scripts/run-live-gate.sh
./scripts/package-client.sh
```

`verify-client-local.sh` 覆盖协议、Qt smoke 和 UI flow；`run-live-gate.sh` 覆盖
真实 gateway/backend 多客户端闭环；`package-client.sh` 验证 Release 构建和 CPack
产物生成。
