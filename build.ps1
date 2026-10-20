param(
    [string]$Toolchain,
    [string]$WinrtHeaders,
    [string]$SdkHeaders
)
$ErrorActionPreference='Stop'
if (-not $Toolchain -or -not $WinrtHeaders -or -not $SdkHeaders) {
    throw '请指定便携 llvm-mingw 根目录、C++/WinRT 投影头文件目录，以及包含 xamlom.h 的 SDK 目录。'
}
$taskCompiler=Join-Path $Toolchain 'bin\x86_64-w64-mingw32-clang++.exe'
$taskArguments=@('-std=c++20','-O2','-shared','-static','-fms-extensions','-DUNICODE','-D_UNICODE','-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00',"-I$WinrtHeaders","-I$SdkHeaders",(Join-Path $PSScriptRoot 'FreeLayout.cpp'),(Join-Path $PSScriptRoot 'FreeLayout.def'),'-o',(Join-Path $PSScriptRoot 'FreeLayout.dll'),'-lole32','-loleaut32','-luuid','-lruntimeobject','-lkernel32','-luser32','-Wl,--exclude-all-symbols')
& $taskCompiler @taskArguments
if ($LASTEXITCODE -ne 0) { throw '原生扩展编译失败。' }
$taskCsc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $taskCsc /nologo /target:winexe /platform:x64 "/win32manifest:$(Join-Path $PSScriptRoot 'app.manifest')" "/out:$(Join-Path $PSScriptRoot 'StartMenuFreeLayout.exe')" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll (Join-Path $PSScriptRoot 'Launcher.cs') (Join-Path $PSScriptRoot 'Startup.cs') (Join-Path $PSScriptRoot 'ModePolicy.cs')
if ($LASTEXITCODE -ne 0) { throw '控制窗口编译失败。' }
Write-Output '构建完成。'
