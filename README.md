# StartMenuFreeLayout

让 Windows 11 **原生开始菜单**的固定图标像桌面一样按格子自由摆放：允许留空、不自动补位，拖动和单击启动同时可用。直接作用于系统开始菜单，不另建启动面板，不需要 Windhawk。

当前版本：**v3.3.2**。发布包默认 **8 列 × 7 行，共 56 格**；在已验证设备上每格为 96 × 84 个逻辑像素。

## 下载

前往 [Releases](https://github.com/12365adwd/StartMenuFreeLayout/releases) 下载 `StartMenuFreeLayout-v3.3.2-win-x64-8x7.zip`。GitHub 自动生成的 Source code 压缩包只有源码，不包含可运行程序。

[直接下载可运行 ZIP](https://github.com/12365adwd/StartMenuFreeLayout/raw/refs/heads/main/dist/StartMenuFreeLayout-v3.3.2-win-x64-8x7.zip) · [SHA256 校验值](dist/SHA256SUMS.txt)

## 功能

- 单击打开快捷方式，不按 Ctrl 也能直接拖动；拖动松开不会误启动应用。
- 吸附网格，可留空位，不交换、不自动补齐图标。
- 应用拖到另一应用上创建原生文件夹，拖到已有文件夹上加入。
- 保留原生文件夹缩小提示及打开动画；文件夹变化时其他图标保持静止。
- 文件夹只剩一个成员时自动解散，成员继承文件夹当前格子。
- 可选登录自启动，记住上次模式和图标位置；开始菜单进程重建后自动恢复。
- 可随时还原系统布局。

## 适配范围

**这是针对下列精确版本适配的实验性原生扩展，并非通用 Windows 11 美化工具。**

| 项目 | 已验证版本 |
| --- | --- |
| 系统 | Windows 11 专业版 25H2，26200.7840，x64 |
| StartMenuExperienceHost.exe | 10.0.26100.7309 |
| StartMenu.dll | 2125.34600.10.0 |

程序会核对系统、开始菜单主程序及组件 SHA256；不匹配时拒绝加载。Windows 更新后可能需要重新适配，不建议移除校验强行使用。

## 使用方法

1. 下载发布包，**解压到一个固定文件夹**；不要直接在压缩包内运行。
2. 双击 `StartMenuFreeLayout.exe`，点击“自由拖动＋点击”。
3. 打开 Windows 开始菜单，按住图标拖到目标格子，松开后保存；单击仍正常打开应用。
4. 需要登录后自动恢复时，勾选“开机自启动：登录后静默恢复上次模式和图标位置”。**发布包默认关闭自启动。**
5. 暂停自定义布局时，点击“还原系统布局”。

四种模式：

| 模式 | 拖动 | 单击启动 |
| --- | --- | --- |
| 自由拖动＋点击 | 直接拖动 | 支持 |
| 锁定位置使用 | Ctrl＋拖动 | 支持 |
| 只整理图标 | 直接拖动 | 暂停 |
| 还原系统布局 | Windows 原生行为 | Windows 原生行为 |

### 文件夹

把应用拖到另一应用所在的格子，停留可看到系统缩小提示，松开后创建文件夹。拖到已有文件夹上可加入；使用原生文件夹界面改名或移出成员。仅剩一个成员时自动取消文件夹，剩余成员留在文件夹原位置。不支持文件夹嵌套。

### 自启动和模式记忆

每次切换模式、完成拖动都会立即保存，不依赖正常关机。开启后使用当前用户的 Windows 登录启动项，登录时静默恢复上次模式及位置，不需要管理员权限。若最后选择“还原系统布局”，下次登录也保持系统布局。

自启动绑定解压路径。移动程序目录前先关闭自启动并还原布局，重启开始菜单或电脑后，从新目录重新启用。不要同时运行多个目录的副本。

### 调整行数

`layout.ini` 中的 `GridRows=7` 将可用行数限制为 7；改为 `0` 可按可见高度自动计算。本机自动计算为 8 行。列数由原生固定区宽度和格子尺寸计算，本机为 8 列；并非所有显示设置下都固定为 8 列。

设置实时读取，通常无需重启。减少行数时，越界图标会移入剩余空格；空间不足则停止自定义布局。调整前可备份 `layout.ini`。

## 停用与还原

1. 取消勾选“开机自启动”。
2. 点击“还原系统布局”。关闭控制窗口本身不会停用扩展。
3. 下次重启电脑后代码彻底卸载且不会自动加载；也可重建开始菜单进程完成卸载。

还原不会删除已保存的位置，也不会更改用户通过组策略隐藏“全部”列表的设置。

## v3.3.2 更新

- 关闭自由布局期间外层固定列表及容器的布局过渡，解决应用移入、移出文件夹时无关图标短距离左右滑动；保留原生文件夹控件自身的动画。
- 还原系统布局时恢复原属性值与样式继承。
- 包含 v3.3.1 的文件夹重复图标修复：先按内部 ID 确认已加入目标，再移除外层项，最终核对成员数量；失败时保留来源。
- 保留单成员文件夹自动解散、图标位置稳定、点击/拖动区分及小栈线程崩溃修复。

## 验证与限制

在上述设备版本中，用户已实际确认自由拖动、单击启动、底行完整显示、原生文件夹创建/加入/解散、重复图标修复及无关图标静止。编译、网格与边界检查、小栈线程回归检查通过；接口检查覆盖插入失败时保留来源、重复操作、身份歧义和 COM 引用计数。

自启动入口已通过重建开始菜单进程验证模式恢复；**没有进行整台电脑重启验证**。未声称已覆盖其他系统版本或所有长期使用场景。

保留原生 ItemsWrapGrid，用 Translation 显示自由位置；内部顺序仍由 Windows 管理，键盘导航可能遵循原始顺序。没有提供隐藏“已固定”标题或单独调整图标大小的功能。

## 源码与构建

- `FreeLayout.cpp` / `FreeLayout.def`：XAML 诊断接入、原生容器布局、手势与动画控制。
- `NativeFolders.h`：与目标组件精确匹配的私有 WinRT 文件夹接口。
- `GridLayout.h` / `PointerGesture.h`：网格及点击/拖动判定。
- `Launcher.cs` / `Startup.cs` / `ModePolicy.cs`：控制窗口、可选自启动与模式持久化。
- `tests/`：网格、手势、模式、小栈线程和文件夹接口检查。
- `layout.ini`：干净初始配置，不含个人图标位置。
- `build.ps1`：构建脚本。

构建准备：

1. [LLVM-MinGW 20261006](https://github.com/mstorsjo/llvm-mingw/releases/tag/20261006)，使用 x64 UCRT 编译器。
2. [Microsoft.Windows.CppWinRT 2.0.250303.1](https://www.nuget.org/packages/Microsoft.Windows.CppWinRT/2.0.250303.1)，从本机 `Windows\System32\WinMetadata` 生成投影头文件。
3. [Microsoft.Windows.SDK.CPP 10.0.26100.7627](https://www.nuget.org/packages/Microsoft.Windows.SDK.CPP/10.0.26100.7627)，准备包含 `xamlom.h` 的目录。
4. Windows 自带的 .NET Framework 4.x x64 C# 编译器。

```powershell
.\build.ps1 -Toolchain 'C:\Dev\llvm-mingw' -WinrtHeaders 'C:\Dev\winrt-headers' -SdkHeaders 'C:\Dev\sdk-headers'
```

不要覆盖仍被开始菜单加载的 DLL。先停用自启动并还原，退出相关进程后再重新构建。

实现参考：[微软 XAML 诊断接口](https://learn.microsoft.com/en-us/windows/win32/api/xamlom/)、[UWPSpy](https://github.com/m417z/UWPSpy)、[ItemContainerTransitions](https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.controls.itemscontrol.itemcontainertransitions)。私有接口依据与目标组件匹配的微软公开 PDB 核对，运行时通过 QueryInterface 调用，不调用实现地址。

## 数据与许可

运行时不联网、不覆盖系统文件。布局保存在同目录 `layout.ini`；诊断日志可能包含应用名称、路径和位置，仅写入本地。仅在勾选自启动时注册自己的当前用户登录项。

仓库和发布包不包含作者个人布局、升级备份、运行日志或 Windows 固定项数据文件。

[MIT License](LICENSE.txt)。
