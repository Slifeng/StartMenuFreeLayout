# StartMenuFreeLayout v0.0.0

<img src="StartMenuFreeLayout/assets/AppIcon.png" alt="StartMenuFreeLayout 图标" width="80">

在 Windows 11 原生开始菜单中按网格自由摆放固定图标，允许留空。支持单击启动、直接拖动、原生文件夹、方向键导航及可选自启动，保留上次模式与位置。

## 下载与使用

[下载 ZIP（程序与源码）](https://github.com/Slifeng/StartMenuFreeLayout/archive/refs/tags/v0.0.0.zip) · [发布页](https://github.com/Slifeng/StartMenuFreeLayout/releases/tag/v0.0.0)

只解压一次，文件结构如下：

```text
开始使用.txt
StartMenuFreeLayout/
  StartMenuFreeLayout-v0.0.0-x64.msi
  启动开始菜单自由布局.exe
  卸载.exe
  SHA256SUMS.txt
  源码及图标资源……
```

先阅读外层 `开始使用.txt`，再进入 `StartMenuFreeLayout` 文件夹，双击 MSI 安装，然后运行启动器。需要卸载时运行同目录 `卸载.exe`，或使用控制窗口的卸载入口。

使用说明包含通过组策略隐藏“所有程序”列表及恢复显示的步骤。程序不会自动修改该策略。已安装旧编号版本时，请先备份需要的布局并用旧版卸载程序卸载，再安装本版；卸载会清除本软件布局数据。

## 功能与适配

- 默认 8×7 自由网格，空位不自动补齐；列数按可用宽度计算，显示设置可能改变列数。
- 浅色卡片控制窗口、四种模式、自启动开关、运行信息及统一蓝色八格图标。
- 原生文件夹创建、加入和动画；单成员文件夹自动解散；方向键跟随当前格子导航。
- 当前用户固定目录安装，下载的安装包和启动器可以移动；卸载清除软件组件、数据、缓存、日志、自启动和私有登记。

适用于通过兼容性检查的 Windows 11 x64 环境，当前为实验性适配，不支持所有系统和开始菜单组件。Windows 更新后可能需要重新适配。保留“全部”应用列表时完整 8×7 空间尚未实际验证。

运行组件位于 `%LOCALAPPDATA%\StartMenuFreeLayout\App\0.0.0`，数据位于同产品目录的 `Data` 子目录。Windows 已固定应用、原生文件夹及手动设置的组策略不会随卸载删除。

## 源码

源码与构建脚本位于 `StartMenuFreeLayout/`，图标资源在其中的 `assets/`，回归检查在 `tests/`。依赖与构建方法见外层使用说明。运行时不联网、不覆盖系统文件；发布包不含个人布局、日志或设备资料。

[MIT License](StartMenuFreeLayout/LICENSE.txt)。本 README 仅供仓库页面展示，不进入下载压缩包。
