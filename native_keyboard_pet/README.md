# 火焰小蓝轻量桌宠

根据用户提供的串珠照片重新绘制为像素动画桌宠。程序使用原生 Win32、GDI+ 和缓存帧，不依赖 Python、Electron 或 WebView。

## 状态切换

- **怒敲键盘**：检测到全局按键活动后立即开始，打字越密集动画越快；约 1.2 秒没有新按键后停止。
- **悄悄卖萌**：没有持续打字时保持安静，每约 10 秒眨眼、微笑或挥手一次。
- **憨憨大睡**：整台电脑连续约 45 秒没有任何操作时睡觉，重新操作后立即醒来。

程序不再安装全局鼠标钩子，也不会响应鼠标使用动作。键盘监听只记录按键活动时间；代码不读取、不保存、不上传具体按键、输入文字、窗口标题、鼠标坐标或剪贴板内容。睡眠判断只读取 Windows 提供的整机空闲时长。

## 互动

- 单击桌宠：立即卖萌。
- 左键拖动：移动到喜欢的位置。
- 右键：切换小号 96px、标准 128px或大号 160px，也可以立即卖萌、回到桌面右下角或退出。
- 命名互斥量保证只运行一个实例。

全新启动默认标准 128px；尺寸选择保存在 `%LOCALAPPDATA%\FlameKeyboardPet\settings.ini`，下次启动自动恢复。旧版保存的 72px 会自动回到标准，旧版 96px 现在对应小号。切换尺寸时保持角色脚底位置，窗口不会突然向下跳。

## 构建与运行

```powershell
.\build.ps1
```

构建会运行状态单元测试和真实 `SendInput -> 低级键盘钩子 -> 打字动作` 集成测试，并验证普通鼠标活动不会切换动作。产物位于 `build\flame-keyboard-pet.exe`，运行时需要保留同目录下的 `assets\actions` 文件夹。也可以双击 `run.bat`。

构建完成后还会生成可直接转发给 Windows 10/11 64 位用户的 `release\FlameKeyboardPet-Windows-x64.zip`。对方先完整解压，再双击 `START.bat`；压缩包内同时提供当前用户免管理员权限的开机启动启用/取消脚本和 SHA-256 校验文件。包内文件名使用纯 ASCII，避免旧版 PowerShell 或不同系统代码页产生路径乱码。

用户选定的四帧小鼠标垫版本作为独立 Mouse Edition 保留，不改变默认无鼠标版：

```powershell
.\build-mouse-edition.ps1
```

其独立产物为 `release\FlameKeyboardPet-Mouse-Edition-Windows-x64.zip`，使用不同的 EXE、窗口类、单实例互斥量和开机启动快捷方式名称，可与默认版分开保存和配置。

## 素材说明

- `assets/source/sprite-sheet-chroma.png`：使用内置图像生成工具和用户照片生成的原始动画表。
- `assets/source/sprite-sheet-transparent.png`：去除背景后的中间素材。
- `tools/process_sprite_sheet.py`：将动画表拆成 12 张、统一尺寸和底线，并清理透明边缘碎片。
- `assets/source/mouse-sheet-selected-chroma.png`：用户明确选定、仅供独立 Mouse Edition 使用的四帧鼠标动作表。
- `tools/process_selected_mouse_sheet.py`：去除选定动作表的洋红背景并生成 Mouse Edition 的四张鼠标帧。
- `assets/actions/*.png`：程序实际使用的 12 张动画帧；删除后程序无法启动。
- `assets/animation-preview.png`：三组动画的静态预览，删除不影响运行。

当前素材用于个人原型。若图片角色属于第三方作品，公开发布或商业使用前需确认相应授权。
