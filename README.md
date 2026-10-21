# StartMenuFreeLayout v3.4.0

让 Windows 11 原生开始菜单的固定图标像桌面一样按格子自由摆放。默认 8 列 × 7 行，可留空位；单击启动、直接拖动同时可用，不替换系统开始菜单。

## 下载最新版

[下载 v3.4.0 可运行压缩包](https://github.com/Slifeng/StartMenuFreeLayout/raw/refs/heads/main/dist/StartMenuFreeLayout-v3.4.0-win-x64-8x7.zip) · [版本发布页](https://github.com/Slifeng/StartMenuFreeLayout/releases) · [SHA256 校验](dist/SHA256SUMS.txt)

压缩包包含 MSI 安装包、可移动启动器、卸载程序、源码和使用说明。GitHub 自动生成的 Source code 压缩包仅含仓库文件；实际安装请使用上述可运行压缩包。

## v3.4 修复

旧版在解压目录加载 DLL，并把自启动写成该目录的绝对路径。移动文件夹后，旧地址失效；已加载的 DLL 还可能被开始菜单占用。

新版改用 Windows 标准 MSI 安装：运行组件和数据保存到当前用户的固定目录，安装包及附带启动器可以任意移动。增加系统“已安装的应用”卸载入口，以及控制窗口的“卸载并清除软件数据”按钮。卸载由 Windows Installer 收尾，不使用 PowerShell、JScript、自删除脚本或临时清理程序。

## 安装和使用

1. 解压发布包，双击 `StartMenuFreeLayout-v3.4.0-x64.msi` 完成当前用户安装。
2. 在开始菜单中搜索 `StartMenuFreeLayout`，或双击随包附带的 `启动开始菜单自由布局.exe`。
3. 点击“自由拖动＋点击”，打开开始菜单拖动图标。空格不会自动补齐；单击应用仍可启动。
4. 按需勾选“开机自启动”。切换模式和拖动结束会立即保存，登录后恢复上次模式和位置。新安装默认关闭，升级保留原先已开启的自启动。

安装包和附带启动器不参与 DLL 加载，移动、改名或删除它们不会影响已安装程序。固定运行目录位于 `%LOCALAPPDATA%\StartMenuFreeLayout`；请通过卸载或重新安装管理该目录。

| 模式 | 拖动 | 单击启动 |
| --- | --- | --- |
| 自由拖动＋点击 | 直接拖动 | 支持 |
| 锁定位置使用 | Ctrl＋拖动 | 支持 |
| 只整理图标 | 直接拖动 | 暂停 |
| 还原系统布局 | Windows 原生行为 | Windows 原生行为 |

应用拖到另一应用上可创建原生文件夹，拖到已有文件夹可加入。保留原生动画；文件夹仅剩一项时自动解散，剩余图标继承文件夹的格子。不支持嵌套文件夹。

## 数据位置和卸载

- 程序：`%LOCALAPPDATA%\StartMenuFreeLayout\App\3.4.0`
- 布局、模式：`%LOCALAPPDATA%\StartMenuFreeLayout\Data\layout.ini`
- 日志和运行数据：同一个 `Data` 目录。
- 自启动：当前用户 `Software\Microsoft\Windows\CurrentVersion\Run` 中的 `StartMenuFreeLayout` 值。
- 私有登记：当前用户 `Software\StartMenuFreeLayout`，以及 Windows Installer 管理的产品卸载登记。

卸载方法任选一种：控制窗口点击“卸载并清除软件数据”；Windows 设置 → 应用 → 已安装的应用 → StartMenuFreeLayout → 卸载；或运行随包的 `卸载.exe`。

卸载会停用自启动、退出本软件的后台程序，释放加载了本软件 DLL 的当前用户开始菜单进程，然后清除运行组件、布局数据、缓存、日志、软件私有注册项及开始菜单中的控制程序快捷方式。开始菜单会由 Windows 重新建立。

你在 Windows 中固定的应用、创建的原生文件夹、通过组策略隐藏“全部”列表的设置会保留。你另存的安装包、源码、旧版解压目录及 Windows 自身的安装历史不会自动删除。旧版目录应在旧 DLL 释放后手动清理。

为避免误删，卸载核对固定路径和安装所有权标识；目录包含符号链接或目录联接时会停止并提示错误，不沿链接删除。

### 还原后只看到两行？

这是系统固定区的折叠视图。点击右上角“全部显示”，按钮变为“显示简略信息”后即可显示全部固定项。折叠时拖动可能临时展开、松手后收回；这不代表图标被删除。还原系统布局后，Windows 原生的拖动排序和创建文件夹功能仍可使用。

## 从旧版本升级

安装时优先保留已经存在的新数据；首次安装会尝试从旧自启动指向的目录导入 `layout.ini`。原文件已经删除时，只能从你保存的备份恢复，无法恢复没有备份的最后一次位置。

可用标准安装参数指定备份：

```powershell
msiexec /i "StartMenuFreeLayout-v3.4.0-x64.msi" IMPORTCONFIG="D:\备份\layout.ini"
```

安装后行数设为 7；其他模式及图标格子沿用导入配置。不要并行运行旧版副本。

## 适配范围

实验性精确适配：Windows 11 专业版 25H2 **26200.7840 x64**；`StartMenuExperienceHost.exe` **10.0.26100.7309**；`StartMenu.dll` **2125.34600.10.0**。

加载前核对版本、进程路径及系统组件 SHA256。不匹配会拒绝加载；Windows 更新后可能需要重新适配。

列数按固定区宽度计算，已验证设备为 8 列；`GridRows=7` 限制为 7 行。显示设置变化可能改变可用列数。内部仍使用原生 ItemsWrapGrid 和 Translation，键盘导航可能遵循系统内部顺序。未提供隐藏“已固定”标题或独立调整图标大小的功能。

## 源码与构建

`FreeLayout.cpp` 处理原生菜单布局及手势；`NativeFolders.h` 为精确匹配的文件夹接口；`Launcher.cs`、`Startup.cs` 管理模式与可选自启动；`Deployment.cs` 处理迁移、所有权核对和卸载准备；`package-msi.ps1` 使用 Windows Installer COM 和系统 MakeCab 构建安装包；`StartInstalled.cs` 是可移动启动器。

依赖：LLVM-MinGW x64 UCRT、C++/WinRT 投影头文件、包含 `xamlom.h` 的 Windows SDK 头文件、Windows 自带 .NET Framework 4.x x64 C# 编译器。

```powershell
.\build.ps1 -Toolchain 'C:\Dev\llvm-mingw' -WinrtHeaders 'C:\Dev\winrt-headers' -SdkHeaders 'C:\Dev\sdk-headers'
```

`tests/deployment-tests.ps1 -TestBase <独立测试目录>` 构建专用 LifecycleTest 安装包，检查迁移、移动安装包、模式和自启动、拒绝目录联接、完整卸载及保留无关文件。测试版本使用独立产品名、安装目录和注册项，不加载原生扩展。其他回归检查位于 `tests/`。

运行时不联网、不覆盖系统文件。发布包不包含个人布局或日志。安装生命周期已用隔离版本实际验证，未进行整机重启测试。

[MIT License](LICENSE.txt)。原生接入参考：[微软 XAML 诊断接口](https://learn.microsoft.com/en-us/windows/win32/api/xamlom/)。安装生命周期依据 [Windows Installer](https://learn.microsoft.com/en-us/windows/win32/msi/about-windows-installer) 的文件和注册表管理机制。
