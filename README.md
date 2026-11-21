# uContra

用 C++11 和 SDL2 复刻初代《魂斗罗》的玩法，以北美 NES 版为基准。程序沿用 `uMario` 的分工：`Application` 负责 SDL 和主循环，`MenuManager` 管理菜单和游戏状态，`Map` 管理当前关卡及对象，`Player` 处理玩家输入、移动和射击。`Stage` 只负责加载和绘制关卡数据，不接管游戏循环。

## 编译与运行

需要 CMake 3.10 或更新版本、支持 C++11 的编译器、GNU Make、SDL2 和 Python 3（带 Pillow）。Ubuntu/Debian 可安装：

```sh
sudo apt install build-essential cmake libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev python3-pil
make assets && make build && make run
```

首次运行 `make assets` 需要联网；它准备八关的背景、碰撞、敌人出生表和精灵图集，不下载 ROM。`make build` 可以在不下载素材的情况下完成，但游玩需要关卡素材。若从终端直接启动程序，请在 `build/` 目录下运行 `./uContra`。

更新后请重新执行 `make assets`，以更新出生表和炮台图集。标题页显示 `START`、`OPTIONS` 和 `ABOUT`，不显示人数选项。确认关卡前不播放音乐；确认时立即播放一次开场曲，黄色关卡编号闪三次后切到比分页；分数数字继续闪动，同一段开场曲结束后静默 0.5 秒，再进入关卡并播放关卡曲。

## 按键

- 主菜单：用 `W/S` 或上下箭头选择菜单项，按 `Enter` 确认；进入 `START` 后用 `A/D` 或左右箭头选择 1–8 关，再按 `Enter` 确认。
- `ABOUT` 循环自动滚动并演示第一关，随机生成小兵、红衣长官和炮台；按 `Esc` 或 `Enter` 返回主菜单，不影响正式关卡进度。
- 游玩：`WASD` 移动／瞄准，`J` 跳跃，`K` 射击。可在标题页或暂停菜单进入按键设置。
- 没拿到 `R` 时，单发武器在屏幕内最多同时存在两发；`S` 的一组散弹按一次发射计算。拿到 `R` 后解除此限制，按住射击键可连发。
- 横版按下可卧倒；在可下穿的平台上按下加跳可落到下一层。在基地先破坏核心，再按上进入下一房间。
- 游玩时按 `Enter` 或 `Esc` 暂停。

## 版本与素材

包含八关、基地房间、Boss、续关和结局界面。关卡动画、敌人行为和时序仍是近似实现，不包含日版 FC 特有的过场与背景动画。素材来源、参考项目及再分发注意事项见 [ASSET_SOURCES.md](ASSET_SOURCES.md)。
