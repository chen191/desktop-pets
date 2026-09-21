# 桌宠源码备份边界

此仓库只备份源码、测试、构建和启动脚本及说明文档，不是可直接运行的完整发行包。
源码与素材在 Git 跟踪层面分离，本机目录及原有运行路径不变。

## 纳入内容

- `native_pet`：吉伊桌宠的 Win32 / GDI+ 源码。
- `native_capybara_pet`：水豚桌宠源码。
- `native_keyboard_pet`：打字桌宠、独立 Mouse Edition、测试及素材处理脚本。
- `sapling_pet`、`app.py`、`tests`：小树苗 Python 原型及测试。

## 不纳入内容

- 除 README 使用的 `docs/screenshots/sapling-pet-preview.png` 实际运行截图外，所有原始图片、图标、精灵图、动作帧及其他预览图均不纳入；相关来源或再分发许可尚未完成确认。
- EXE、编译中间文件、发布 ZIP、录屏、缓存、日志、用户设置和运行数据。
- 本机协作文件、素材审核记录和私有踩坑记录。
- `quick_translate`：另有独立仓库，不作为此仓库的子模块。

不要将源码的备份视为素材授权，也不要为未核实的素材添加开源许可证。
未上传的素材仍仅保留在本机；此仓库不能替代它们的本地备份。

## 恢复后需要补齐的素材

仅在确认拥有相应使用权限后，从本机备份恢复或用原创素材替代：

| 版本 | 所需文件（均未上传） |
|---|---|
| 吉伊 | `native_pet/assets/chiikawa.png` |
| 水豚 | `native_capybara_pet/assets/lulu.png`；`assets/actions/` 下的 `neutral.png`、`blink.png`、`wave.png`、`walk_a.png`、`walk_b.png`、`sleep.png` |
| 打字桌宠 | `native_keyboard_pet/assets/app.ico`；`assets/actions/` 下 `type_0.png`–`type_3.png`、`sleep_0.png`–`sleep_3.png`、`cute_0.png`–`cute_3.png` |
| Mouse Edition | 另需 `native_keyboard_pet/assets/source/mouse-sheet-selected-chroma.png`；构建脚本据此生成 `assets/mouse-edition/mouse_0.png`–`mouse_3.png` |
| 小树苗 | `assets/stages/stage_0_tree.png`–`stage_3_tree.png` 及 `stage_0_base.png`–`stage_3_base.png` |

普通版动作帧处理脚本另需 `native_keyboard_pet/assets/source/sprite-sheet-transparent.png`。
构建脚本目前保留原机 MinGW 路径 `E:\Program Files\mingw64\bin`，换机后需检查并调整。
未补素材时，完整构建、打包或启动可能失败，特别是图标资源编译与图片复制步骤。

## 无素材验证

```powershell
python -B -m unittest discover -s tests -v
```

C++ 状态单元测试可单独编译 `native_keyboard_pet/tests/activity_unit_test.cpp`，
普通版与定义 `FLAME_MOUSE_EDITION` 的版本分别运行，不需要图片。
这不等于桌面交互验证；原生集成测试会启动程序并注入输入，本次源码备份不执行它。

## 后续提交

`.gitignore` 默认拒绝文件，仅放行指定目录中的源码与脚本。
新增文件类型或目录需先审查再调整白名单；不要使用 `git add -f` 绕过素材和秘密信息排除规则。
每次推送前用 `git diff --cached --name-only` 与 `git diff --cached` 核对范围。
