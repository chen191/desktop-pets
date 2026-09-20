# 吉伊轻量桌宠

这是一个原生 Windows 桌宠原型，不使用 Python、Electron 或 WebView。

## 交互

- 默认以缓动方式跟随鼠标，并与光标保持距离。
- 移动时会小步跳跃和左右摇摆，停下后只做轻微呼吸。
- 左键拖动会关闭跟随并手动摆放。
- 右键可以恢复/暂停跟随、回到桌面右下角或退出。
- 程序使用命名互斥量保证只运行一个实例。

## 构建

```powershell
.\build.ps1
```

构建产物位于 `build\chiikawa-pet.exe`，运行时读取 `build\assets\chiikawa.png`。

## 开机启动

便携包中提供两个无需管理员权限的一键脚本：

- 双击 `build\一键启用开机启动.bat`，自动为当前 Windows 用户创建启动快捷方式。
- 双击 `build\一键取消开机启动.bat`，自动删除启动快捷方式，不会删除桌宠文件。

批处理本身只负责调用同目录的 `enable-startup.ps1` 或 `disable-startup.ps1`；实际逻辑使用纯 ASCII 路径和 `Chiikawa Lite Pet.lnk`，避免不同 Windows 代码页造成乱码。脚本通过自身目录定位 `chiikawa-pet.exe`，因此复制到其他电脑后不需要手动修改路径。设置完成后不要随意移动整个 `build` 文件夹；如果移动了，重新运行启用脚本即可更新路径。

角色素材仅用于个人非商用原型；若要发布或商业使用，需要替换为拥有合法授权的原创角色素材。
