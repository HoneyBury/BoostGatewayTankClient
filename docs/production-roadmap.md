# 生产规划路线图

项目大方向是实现一个基于 BoostGateway 服务端和 C++ SDK 的多人在线同屏坦克
大战客户端。它既是可玩的 demo，也是服务端实时框架、SDK、房间、战斗、
排行榜、回放等能力的真实验证入口。

## P0：客户端骨架和 SDK 边界

目标：让客户端工程独立成立。

内容：

- Qt/CMake 工程可独立配置、构建、测试。
- SDK 通过 CMake package 或开发期 fallback 导入。
- 登录、大厅、战斗三个基础窗口存在。
- `tank.input` 编码和 `tank.snapshot` 解码有 smoke test。
- 文档明确客户端/服务端边界。

退出标准：客户端能构建，且不复制服务端源码。

## P1：真实登录和房间大厅

目标：让两个真实客户端可以进入同一房间。

内容：

- 注册/登录窗口完善，支持本地 profile 保存。
- 显示 gateway host/port、连接状态、SDK version、错误信息。
- 房间列表、房间详情、创建房间、加入房间、离开房间。
- 准备/取消准备、房主转让、踢人。
- 后端不可用、超时、房间不存在、房间已满等错误可读。

退出标准：两台或两个本地客户端可以稳定进入同一房间并 ready。

## P2：多人在线战斗 MVP

目标：完成最小可玩的多人同屏战斗闭环。

内容：

- 从房间开始 battle。
- 接收服务端 authoritative snapshot。
- 渲染地图、坦克、子弹、HP、击中事件、死亡和结束状态。
- WASD/方向键移动，空格开火。
- 输入带本地 seq，最终状态以服务端 snapshot 为准。
- 显示 frame、push rate、延迟、frame lag。

退出标准：两个客户端看到同一个战斗状态，并且都能移动和开火。

## P3：断线重连和稳定性

目标：让客户端具备真实网络环境下的可用性。

内容：

- heartbeat 状态显示。
- disconnect UI 和自动重连状态机。
- retry/backoff 和重连次数显示。
- 重新 login / 恢复房间 / 恢复 battle。
- resume snapshot 应用。
- 客户端事件队列隔离 SDK callback 和 UI 更新。

退出标准：在服务端 resume window 内临时断线不会丢失战斗状态。

## P4：排行榜、结算和回放

目标：让完整战斗结果可查询、可展示、可回看。

内容：

- 排行榜 top/rank UI。
- 战斗结算详情页。
- replay 列表/查询入口。
- 回放播放时间轴、暂停、继续、seek、倍速。
- 回放渲染复用战斗渲染模型。

退出标准：一局战斗结束后，客户端能看到结算、排行榜变化，并能回看战斗。

## P5：道具和玩法表现

目标：提升 demo 的游戏完整度。

内容：

- 道具/增益协议 adapter。
- 道具生成、拾取、冷却、buff 状态展示。
- 更好的坦克 sprite、地图 tile、爆炸效果、音效。
- 观战模式预留。
- 简单设置面板：音量、按键、渲染比例、服务器地址。

退出标准：道具玩法在客户端可见、可验证，并且不破坏 SDK 通用边界。

## P6：打包和发布

目标：让非开发环境也能安装运行。

内容：

- macOS/Windows 打包。
- SDK 动态库发现和诊断。
- 版本矩阵：client、SDK、gateway、tank protocol。
- 基础发布说明和升级说明。
- 如有需要，补签名、公证或安装器。

退出标准：非开发用户可以安装客户端并连接测试服务器。

## P7：客户端可观测性和自动化验收

目标：客户端问题可诊断、可复现、可自动验证。

内容：

- 本地日志记录 player id、room id、battle id、请求步骤、错误码。
- 诊断面板显示 ping、frame、push rate、reconnect count、SDK version。
- 可导出 bug report bundle。
- headless bot 或 Qt 集成测试。
- 跨仓联调 summary：启动服务端、两个客户端、完成战斗闭环。

退出标准：客户端问题能通过日志和自动化门禁定位，不再依赖纯手工观察。

## 当前建议优先级

1. P1 真实登录和房间大厅。
2. P2 多人在线战斗 MVP。
3. P3 断线重连和稳定性。
4. P4 排行榜、结算和回放。
5. P5 道具和玩法表现。
6. P6 打包发布。
7. P7 可观测性和自动化验收。

## 当前代码落地状态

- P1 已有注册/登录、大厅、创建/加入/离开、ready/unready、房间列表、房间详情、
  踢出成员、转让房主和排行榜查询入口；注册、房间列表/详情、踢人和转让房主
  已通过 SDK 与真实 live gate 验证。
- P2 已有战斗页、键盘输入、snapshot 渲染、子弹/道具预留和基础战斗指标；
  已兼容当前服务端 `battle_state:kind=...`、`frame_advanced` 和
  `battle_finished` push。Qt UI 的 WASD/方向键已切到真实服务端 `move:x,y`
  输入，空格使用 `attack:user_id`，F 使用 `finish:reason`。
- P3 已有 heartbeat 启动、disconnect callback、菜单重连入口、重连次数诊断、
  `SessionResumedPush` / `SessionKickedPush` 处理和本地 room/battle 上下文弱恢复；
  room detail 已可用于大厅恢复辅助，battle state 查询已接入重连后的主动 snapshot
  恢复；`tank_headless_gate` 已覆盖真断线、重连、重新登录和 snapshot 恢复。后续
  需要补图形界面自动化场景和更细的恢复错误提示。
- P4 已有排行榜页、结算摘要和回放页；排行榜 top/rank 与回放加载走 SDK，
  战斗结束后会自动刷新最近一局结算和排行榜，headless gate 会验证 replay load。
- P5 已有道具 snapshot 模型和渲染占位，具体道具协议等待服务端协议稳定。
- P6 已有构建脚本和 SDK 导入策略，尚未做平台打包。
- P7 已有诊断页、SDK version、push/snapshot/input/reconnect 计数；已新增
  `tank_headless_gate`、`tank_ui_smoke_test`、`scripts/run-headless-gate.sh` 和
  `scripts/run-live-gate.sh`，可覆盖离线 Qt UI smoke 与真实 gateway 双客户端
  login/room/ready/battle/input/finish/leaderboard 验证。

## P1/P2/P7 当前收束结论

当前优先级 1/2/3 的客户端侧已经完成一轮可验证收束：

- 房间链路：现有 SDK 支持的 create/join/leave/ready/start 已接入 UI 和 headless gate。
- 大厅真实化：UI 会阻止未进房时 ready/start/leave 等无效操作；房间列表、
  房间详情、踢出成员和转让房主已走 SDK 访问真实 gateway/backend。
- 战斗联调：客户端可识别现有服务端 `battle_state` 和 `frame_advanced`
  push，并能发送现有 SDK 主链使用的 `move:x,y`、`attack:user_id` 和
  `finish:reason` 输入。
- 同屏画面：Qt UI 已能根据服务端 `participants[].pos_x/pos_y` 渲染玩家位置；
  客户端仍保留 `tank.input` JSON 和 `tank.snapshot` JSON 模型，便于后续协议升级。
- 结算排行榜：客户端可解析 `battle_finished` 的 winner、reason、total_frames 和
  scores，战斗结束后自动切到排行榜页并刷新展示。
- P3 弱恢复：客户端可识别服务端 resumed/kicked push，重连后恢复本地 room/battle
  导航上下文，并通过服务端 SDK `battle_state` 查询恢复最新 authoritative snapshot；
  真断线重连恢复已进入 live gate，后续要把该能力放入图形界面自动化 gate。
- 自动化：`tank_protocol_smoke_test` 覆盖协议解析，`tank_ui_smoke_test` 覆盖离线
  Qt Widgets 构造和战斗快照渲染，`tank_headless_gate` 覆盖真实 gateway 的注册、
  房间管理、战斗、重连恢复、回放和排行榜业务闭环。
