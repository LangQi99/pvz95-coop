# PvZ 95 Co-op

[![Windows and macOS CI](https://github.com/LangQi99/pvz95-coop/actions/workflows/ci.yml/badge.svg)](https://github.com/LangQi99/pvz95-coop/actions/workflows/ci.yml)

[简体中文](#简体中文) | [English](#english)

## 简体中文

> **开发中：** 这是一个面向 Windows 和 macOS、由房主权威驱动的 PvZ 95 社区改版局域网合作实现。目前引擎和测试套件能够构建并运行，但联机玩法尚未达到正式发布标准。

PvZ 95 Co-op 的目标是让 2–4 名玩家共同操控同一局《植物大战僵尸》。每位玩家拥有不同颜色的鼠标指针，以及彼此独立的植物拿取状态和放置预览；房主负责验证并排序语义化游戏操作，以尽量维持各端状态一致。

### 项目进度

- [x] Windows x64 与 macOS Apple Silicon 的 CMake 构建基线
- [x] 可在运行时选择的 PvZ 95 规则集，以及经过验证的直接数据修改
- [x] 带版本号的发现、握手、会话启动、鼠标展示、语义操作、时钟和状态哈希数据包编解码
- [x] 基于 WinSock/BSD Socket 的非阻塞 UDP 局域网发现，以及回环集成测试
- [x] 有界可靠 TCP 通道，以及经过校验的房主/客户端房间会话
- [x] 主菜单中的主持、手动加入和自动搜索控制
- [x] 原版零售/PvZ 95 编译资源兼容与中文文本转换
- [ ] 恢复剩余的 PvZ 95 注入行为，并补充回归测试
- [x] 绘制不同颜色的远端鼠标，以及每位玩家独立、不染色的植物放置预览
- [x] 在不传输原始点击事件的前提下，分发经过校验、由房主权威处理的种植、拾取阳光/金币、铲除和玉米加农炮操作
- [x] 同步会话启动、房主游戏档案、关卡参数与确定性随机状态
- [x] 将已接受的操作安排到房主时钟，并根据权威时钟调节客户端进度
- [x] 使用规范化的周期性棋盘哈希检测确定性状态分歧
- [ ] 加入快照重同步、断线重连和双机长时间压力测试
- [ ] 生成经过签名/公证的 macOS 版本与打包的 Windows 发行版

更多设计细节见[架构说明](docs/ARCHITECTURE.md)，95 版移植依据见[PvZ 95 研究记录](docs/research/PVZ95_RULESET.md)。

### 技术来源与实现方式

本项目结合了两个互补的代码与研究来源：

- [PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable) 提供持续维护的 SDL/OpenGL/CMake 引擎，以及 Windows/macOS 平台层。
- [PlantsVsZombies-decompilation](https://github.com/ruslan831/PlantsVsZombies-decompilation) 仅作为研究参考，用于将 PvZ 95 可执行文件中的补丁映射回有名称的游戏函数。本仓库没有复制其仅限 Windows 的工程、二进制文件或游戏资源。

恢复出的行为会以可维护的 C++ 重新实现，并根据观察到的 PvZ 95 结果进行测试。本项目不尝试逐字节复刻专有机器代码，也不宣称已经完整移植全部 95 版行为。

### 所需游戏资源

本仓库**不包含** PopCap/EA 的游戏资源或原版可执行文件。你可以从第三方提供的[植物大战僵尸 95 版资源包](https://d2.wwh8.net/%E6%A4%8D%E7%89%A9%E5%A4%A7%E6%88%98%E5%83%B5%E5%B0%B895%E7%89%88.zip)获取兼容资源；请仅在你拥有相应游戏合法使用权时下载和使用。该链接及其内容不由本项目托管或维护。

下载与启动：

1. 下载上面的资源包并完整解压。
2. 从本仓库的 [Releases](https://github.com/LangQi99/pvz95-coop/releases/latest) 下载对应平台版本。
3. Windows：把下载的 `PvZ-95-Co-op-*-Windows-x64.exe` 拖进解压后的完整游戏目录，与 `main.pak`、`properties/` 放在同一级，然后双击启动。
4. Apple Silicon macOS：解压下载的 ZIP，把 `pvz95-coop.app` 拖进同一个完整游戏目录，然后双击启动。首次打开可能需要在 Finder 中右键应用并选择“打开”。

加载器支持所分析 PvZ 95 包使用的原版零售 1.0 编译定义，也支持较新的原生布局，并会在加载时把旧版 GBK 中文文本转换为 UTF-8。

也可以在启动时显式指定资源目录：

```bash
./pvz95-coop -resdir /path/to/your/legal/game-data
```

设置与存档使用独立的产品 ID `io.github.langqi99.pvz95-coop`，不会覆盖上游 PvZ-Portable 的用户档案。也可以通过 `-savedir <path>` 指定其他可写目录。

### 在 macOS 上构建

使用 Homebrew 安装依赖：

```bash
brew install cmake ninja sdl2 freetype libpng jpeg-turbo libogg libvorbis libopenmpt mpg123 dylibbundler
```

配置、构建并测试：

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

应用包位于 `build/pvz95-coop.app`。

### 在 Windows 上构建

CI 支持的 Windows 构建环境为 Visual Studio 2022、Ninja、CMake 和 vcpkg 清单模式。在已经设置 `VCPKG_ROOT` 的 Developer PowerShell 中运行：

```powershell
cmake -G Ninja -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCONSOLE=OFF `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build
ctest --test-dir build --output-on-failure
```

可执行文件位于 `build/pvz95-coop.exe`。

### 规则集与联机

程序默认启用 PvZ 95 数值，也保留原版 PvZ 数值用于 A/B 对比：

```bash
./pvz95-coop -ruleset pvz95
./pvz95-coop -ruleset original
```

联机握手会校验规则集协议 ID，使用不兼容规则集的玩家会在游戏开始前被拒绝。

主菜单提供三种局域网操作：

- **Host LAN**：主持房间。默认在 TCP `43096` 端口监听游戏连接，并在房间状态中显示当前/最多人数和实际端口。
- **Join Room**：手动加入。打开可编辑的 IPv4 地址与 TCP 端口输入框，默认值为 `127.0.0.1:43096`。使用端口转发或内网穿透时，请在这里填写其公开 IPv4 地址和对外 TCP 端口。
- **Auto Search**：自动搜索。通过 UDP `43095` 在局域网中发现房间，再连接房主广播的 TCP 游戏端口。

项目目前没有提供公网匹配或自动 NAT 打洞；端口转发与内网穿透服务需要由玩家自行配置。

在 macOS 上进行可重复的本机双开测试时，请为两个实例使用不同的存档目录：

```bash
./pvz95-coop -windowed -savedir /tmp/pvz-host -lan-host
./pvz95-coop -windowed -savedir /tmp/pvz-client -lan-join -lan-address 127.0.0.1
```

省略 `-lan-address` 会使用普通的局域网广播发现。该命令行地址指向 UDP 发现服务；若要直接连接 TCP 地址或互联网隧道，请使用主菜单的 **Join Room**。`-windowed` 会覆盖已保存的显示偏好，使本机两个实例都以独立窗口打开。

### 联机诊断日志

程序会把联机会话、连接状态、权威操作、时钟、状态哈希和不同步检测写入 `lan-sync.log`。日志按大小轮转为当前文件及 `lan-sync.log.1`、`.2`、`.3` 三个备份，每个文件最多 4 MiB，总量约 16 MiB；按正常对局的记录速率可保留远超 30 分钟的诊断信息。重新启动不会清空已有日志。

日志与存档、设置位于同一个可写数据目录。若希望容易找到日志，可使用 `-savedir <path>` 指定目录，例如 Windows PowerShell：

```powershell
.\pvz95-coop.exe -savedir .\pvz95-data
```

发生不同步时，请尽快退出双方游戏，并从房主和客户端各自的数据目录收集全部 `lan-sync.log*` 文件。两端日志必须配对，才能可靠定位最后一个一致的游戏时钟与首次状态分歧。

### 测试

当前原生测试套件覆盖：

- 所有已直接验证的植物与投射物数据修改；
- 每种联机数据包的往返编解码；
- 格式错误、尺寸过大、协议版本不匹配及字段非法的数据包；
- UTF-8 直通、Windows-1252 与旧版中文 GBK 转换；
- 房主与客户端之间真实的 UDP 回环发现；
- 非阻塞 TCP 连接、带帧的双向握手流量和对端关闭检测；
- 坐标归一化、带颜色的鼠标与手持植物展示状态、过期/回绕序列号处理和玩家移除；
- 房主/客户端端到端加入、规则集拒绝、玩家 ID 绑定、已接受操作的转发、鼠标序列和房主广播；
- 会话启动/就绪/开始屏障，以及房主游戏档案数据包校验；
- 未来时钟上的稳定操作排序、重复/过期/容量处理和规范化哈希基础组件。

协议不会发送原生 C++ 结构体布局，而是使用显式的固定宽度小端字段，并在使用前校验数据包长度、枚举值和玩家范围。

### 法律声明与致谢

PvZ 95 Co-op 是非官方、非商业的教育与研究项目，与 PopCap Games 或 Electronic Arts 无关联，也未获得其授权或背书。

源代码沿用 PvZ-Portable 的 [LGPL-3.0](LICENSE) 许可。本仓库不分发任何 PopCap/EA 游戏资源。为了让研究可复现，仓库记录了 PvZ 95 样本的指纹，但样本本身不会纳入 Git。

---

## English

> **Work in progress:** This is a Windows/macOS, host-authoritative LAN co-op implementation of the community PvZ 95 ruleset. The engine and test suite build and run, but multiplayer gameplay is not release-ready yet.

PvZ 95 Co-op aims to let two to four people control one shared Plants vs. Zombies game. Each player gets a separately colored pointer and an independent plant-in-hand/placement preview; the host validates and orders semantic game actions to keep peers aligned as closely as possible.

### Project status

- [x] Windows x64 and macOS Apple Silicon CMake baseline
- [x] Runtime-selectable PvZ 95 ruleset and verified direct data changes
- [x] Versioned discovery, handshake, session-start, cursor-presentation, semantic-action, tick, and state-hash packet codec
- [x] Non-blocking WinSock/BSD Socket UDP LAN discovery with loopback integration tests
- [x] Bounded reliable channel plus validated host/client lobby sessions over TCP
- [x] Host, direct-join, and automatic-search controls on the main menu
- [x] Original retail/PvZ 95 compiled-resource compatibility and Chinese text conversion
- [ ] Restore the remaining injected PvZ 95 behavior hooks with regression tests
- [x] Render colored remote pointers plus independent untinted plant previews for every player
- [x] Dispatch validated, host-authoritative plant/coin/shovel/cob-cannon actions without networking raw clicks
- [x] Synchronize session start, host gameplay profile, level parameters, and deterministic random state
- [x] Schedule accepted actions on a host tick and pace clients from the authoritative tick stream
- [x] Detect deterministic-state divergence with a canonical periodic board hash
- [ ] Add snapshot resynchronization, reconnect, and two-machine soak tests
- [ ] Produce signed/notarized macOS and packaged Windows releases

See the [architecture](docs/ARCHITECTURE.md) and [PvZ 95 research notes](docs/research/PVZ95_RULESET.md) for more detail.

### Why this codebase

This project combines two complementary sources:

- [PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable) supplies the maintained SDL/OpenGL/CMake engine and the Windows/macOS platform layer.
- [PlantsVsZombies-decompilation](https://github.com/ruslan831/PlantsVsZombies-decompilation) is used only as a research reference to map PvZ 95 executable patches back to named game functions. Its Windows-only project, binaries, and game assets are not copied here.

Recovered behavior is implemented as maintainable C++ and tested against observed PvZ 95 outcomes. The project does not attempt to reproduce proprietary machine code byte-for-byte and does not claim that every PvZ 95 behavior has already been ported.

### Required game data

This repository does **not** contain PopCap/EA game assets or original executables. Compatible data is available from this third-party [Plants vs. Zombies 95 resource archive](https://d2.wwh8.net/%E6%A4%8D%E7%89%A9%E5%A4%A7%E6%88%98%E5%83%B5%E5%B0%B895%E7%89%88.zip); download and use it only if you are legally entitled to use the corresponding game. This project neither hosts nor maintains the link or its contents.

To download and run the game:

1. Download the resource archive above and extract it completely.
2. Download the package for your platform from this repository's [Releases](https://github.com/LangQi99/pvz95-coop/releases/latest).
3. Windows: move `PvZ-95-Co-op-*-Windows-x64.exe` into the extracted game folder, alongside `main.pak` and `properties/`, then double-click it.
4. Apple Silicon macOS: extract the downloaded ZIP, move `pvz95-coop.app` into the same complete game folder, then double-click it. On first launch, you may need to right-click the app in Finder and choose **Open**.

The loader accepts the original retail 1.0 compiled definitions used by the analyzed PvZ 95 package as well as the newer native layout, and converts legacy Chinese GBK text to UTF-8 at load time.

Alternatively, launch with an explicit resource directory:

```bash
./pvz95-coop -resdir /path/to/your/legal/game-data
```

Writable settings and saves use the separate product ID `io.github.langqi99.pvz95-coop`, so they do not overwrite an upstream PvZ-Portable profile. You can override that location with `-savedir <path>`.

### Build on macOS

Install dependencies with Homebrew:

```bash
brew install cmake ninja sdl2 freetype libpng jpeg-turbo libogg libvorbis libopenmpt mpg123 dylibbundler
```

Configure, build, and test:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The app bundle is `build/pvz95-coop.app`.

### Build on Windows

The CI-supported Windows build uses Visual Studio 2022, Ninja, CMake, and vcpkg manifest mode. In a Developer PowerShell with `VCPKG_ROOT` set:

```powershell
cmake -G Ninja -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCONSOLE=OFF `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build
ctest --test-dir build --output-on-failure
```

The executable is `build/pvz95-coop.exe`.

### Ruleset and multiplayer

PvZ 95 values are enabled by default. Original PvZ values remain available for A/B comparison:

```bash
./pvz95-coop -ruleset pvz95
./pvz95-coop -ruleset original
```

The ruleset protocol ID is part of the multiplayer handshake. Peers with incompatible gameplay rules are rejected before starting a session.

The main menu exposes three LAN actions:

- **Host LAN** hosts a room. It listens for game traffic on TCP port `43096` by default and shows the current/maximum player count and actual port in the room status.
- **Join Room** connects directly. It opens an editable IPv4 address and TCP port dialog, prefilled with `127.0.0.1:43096`. For a tunnel or port-forwarded game, enter its public IPv4 address and exposed TCP port here.
- **Auto Search** uses UDP port `43095` for zero-configuration LAN discovery, then connects to the TCP game port advertised by the host.

The project does not currently provide public matchmaking or automatic NAT traversal. Players must configure any port forwarding or tunneling service themselves.

For repeatable local two-instance testing on macOS, use a different save directory for each instance:

```bash
./pvz95-coop -windowed -savedir /tmp/pvz-host -lan-host
./pvz95-coop -windowed -savedir /tmp/pvz-client -lan-join -lan-address 127.0.0.1
```

Omit `-lan-address` to use normal LAN broadcast discovery. This command-line address targets the UDP discovery service; use the main-menu **Join Room** dialog for a direct TCP connection or an Internet tunnel. `-windowed` overrides the saved display preference, so two local test instances open as independent windows.

### Multiplayer diagnostic logs

The game records session lifecycle, connection state, authoritative actions, ticks, state hashes, and desync detection in `lan-sync.log`. It rotates by size into the active file plus `lan-sync.log.1`, `.2`, and `.3`. Each file is capped at 4 MiB (about 16 MiB total), which retains well over 30 minutes at the normal gameplay trace rate. Existing logs are preserved across restarts.

Logs share the writable data directory used by saves and settings. To make them easy to find, select that directory with `-savedir <path>`, for example in Windows PowerShell:

```powershell
.\pvz95-coop.exe -savedir .\pvz95-data
```

After a desync, exit both games promptly and collect every `lan-sync.log*` file from both the host and client data directories. The paired logs are needed to identify the last matching game tick and the first state divergence reliably.

### Tests

The current native test suite covers:

- all directly verified plant and projectile table changes;
- round-trip encoding of every multiplayer packet type;
- malformed, oversized, mismatched-version, and invalid-field packets;
- UTF-8 passthrough plus Windows-1252 and legacy Chinese GBK conversion;
- real UDP discovery between a host and client over loopback;
- non-blocking TCP connection, framed two-way handshake traffic, and peer-close detection;
- coordinate normalization, colored cursor and held-seed presentation state, stale/wrapped sequence handling, and player removal;
- end-to-end host/client join, ruleset rejection, player-ID binding, accepted-action rebroadcast, cursor sequencing, and host broadcast;
- synchronized session start/ready/begin barriers and host-gameplay-profile packet validation;
- stable future-tick action ordering, duplicate/late/capacity handling, and canonical hash primitives.

The protocol never sends native C++ struct layouts. It uses explicit fixed-width little-endian fields and validates packet length and enum/player ranges before use.

### Legal and attribution

PvZ 95 Co-op is an unofficial, non-commercial educational and research project. It is not affiliated with, authorized by, or endorsed by PopCap Games or Electronic Arts.

The source remains under [LGPL-3.0](LICENSE), following PvZ-Portable. No PopCap/EA resources are distributed. The PvZ 95 sample fingerprint is documented for reproducible research, but the sample itself is excluded from Git.
