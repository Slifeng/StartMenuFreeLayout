# StartMenuFreeLayout v0.0.0

<img src="assets/AppIcon.png" alt="StartMenuFreeLayout 图标" width="80">

让 Windows 11 原生开始菜单的固定图标像桌面一样按格子自由摆放。默认 8 列 × 7 行，允许留空；单击启动、直接拖动同时可用。

## 下载和启动

[下载 ZIP（程序及源码）](https://github.com/Slifeng/StartMenuFreeLayout/archive/refs/tags/v0.0.0.zip) · [发布页](https://github.com/Slifeng/StartMenuFreeLayout/releases/tag/v0.0.0)

**只需要解压一次。** 仓库及压缩包根目录直接包含：

| 文件 | 用途 |
| --- | --- |
| `StartMenuFreeLayout-v0.0.0-x64.msi` | 首次使用先双击安装 |
| `启动开始菜单自由布局.exe` | 安装后打开控制窗口 |
| `卸载.exe` | 卸载程序并清除本软件的数据与自启动项 |
| `开始使用.txt` | 简明使用说明 |
| `SHA256SUMS.txt` | 文件校验值 |

1. 解压 ZIP，双击根目录 MSI 完成当前用户安装。
2. 双击根目录启动器，点击“自由拖动＋点击”。
3. 打开系统开始菜单拖动图标；单击应用仍可启动。
4. 按需开启“开机自启动”，登录后恢复上次模式和位置。新安装默认关闭。

版本编号从 v0.0.0 重新开始。已安装旧编号版本时，请先通过旧版卸载程序卸载，再安装本版；旧版卸载会清除其布局数据，请先自行备份需要的布局文件。不要把本版当作旧编号版本的直接覆盖更新。

## 功能

- 浅色圆角控制窗口、蓝色模式选中卡片、自启动开关、状态栏和可折叠运行信息。
- 透明底蓝色八格图标统一用于程序、窗口、任务栏、快捷方式与安装登记。
- 自由网格保留空位，不自动补齐；拖动不误启动，单击可打开。
- 方向键按当前格子导航并跳过空位；Enter 可打开应用或文件夹。
- 应用重叠创建原生文件夹，拖到文件夹可加入；保留原生动画。只剩一个成员时自动解散，成员留在原格子。不支持嵌套文件夹。

| 模式 | 拖动 | 单击启动 |
| --- | --- | --- |
| 自由拖动＋点击 | 直接拖动 | 支持 |
| 锁定位置使用 | Ctrl＋拖动 | 支持 |
| 只整理图标 | 直接拖动 | 暂停 |
| 系统原生布局 | Windows 原生行为 | Windows 原生行为 |

还原系统布局后只显示两行时，点击右上角“全部显示”展开。原生折叠视图在拖动时可能临时展开；这不代表固定项被删除。

## 安装位置、移动与卸载

- 运行组件：`%LOCALAPPDATA%\StartMenuFreeLayout\App\0.0.0`
- 布局、模式、日志：`%LOCALAPPDATA%\StartMenuFreeLayout\Data`
- 自启动：当前用户 `Software\Microsoft\Windows\CurrentVersion\Run` 中的 `StartMenuFreeLayout` 值。
- 私有登记：当前用户 `Software\StartMenuFreeLayout` 及 Windows Installer 管理的产品登记。

MSI 将运行组件安装到当前用户固定目录。安装后，下载的安装包和启动器可以移动或删除，不影响已安装程序；不要手动移动固定运行目录。源码与安装包不是程序运行数据。

卸载任选一种：运行根目录 `卸载.exe`；控制窗口点击“卸载并清除数据”；或在 Windows“已安装的应用”中卸载。

卸载会停止后台程序和自启动，释放加载了本软件 DLL 的当前用户开始菜单进程，清除运行组件、布局、缓存、日志、软件私有注册项及控制程序快捷方式。Windows 会重新建立开始菜单进程。

Windows 已固定应用、原生文件夹及用户自行设置的组策略保留。用户另存的安装包、源码、备份和 Windows 安装历史不自动删除。清理时核对固定路径和安装所有权，拒绝沿目录联接或符号链接删除。

## 适配范围

适用于通过程序兼容性检查的 Windows 11 x64 环境。当前为实验性适配，不支持所有系统和开始菜单组件。加载前核对平台指纹、进程路径及系统组件 SHA256；不匹配会拒绝加载，Windows 更新后可能需要重新适配。

固定区列数按宽度计算，默认布局为 8 列、`GridRows=7`；显示设置变化可能改变可用列数。未提供隐藏“已固定”标题或单独调整图标大小的功能。

## 源码与构建

源码直接位于根目录，图标资源位于 `assets/`，回归检查位于 `tests/`。`ControlWindow.cs` 负责控制窗口；`FreeLayout.cpp` 负责原生布局；`SpatialNavigation.h` 计算方向键目标；`Deployment.cs` 管理安装与卸载；`StartInstalled.cs` 是可移动启动器。

依赖 LLVM-MinGW x64 UCRT、C++/WinRT 投影头文件、包含 `xamlom.h` 的 Windows SDK 头文件、Windows 自带 .NET Framework 4.x x64 C# 编译器。

```powershell
.\build.ps1 -Toolchain 'C:\Dev\llvm-mingw' -WinrtHeaders 'C:\Dev\winrt-headers' -SdkHeaders 'C:\Dev\sdk-headers'
```

构建后复制 `Uninstall.exe` 为对外分发的 `卸载.exe`；根目录保留 MSI、中文启动器和卸载器，运行组件打入 MSI。

`tests/deployment-tests.ps1 -TestBase <独立测试目录>` 用独立产品名、目录及注册项检查安装、迁移、移动安装包、自启动和卸载，不加载原生扩展。

运行时不联网、不覆盖系统文件。发布包不包含个人布局或日志。[MIT License](LICENSE.txt)。
