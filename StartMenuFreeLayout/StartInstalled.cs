// MIT. A movable launcher; the installed app owns the runtime DLL.
using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;
static class StartInstalled
{
    [STAThread] static int Main()
    {
        string path=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),@"StartMenuFreeLayout\App\0.0.0\StartMenuFreeLayout.exe");
        if(!File.Exists(path)){MessageBox.Show("请先运行压缩包中的 MSI 安装包。安装后，这个启动文件可以任意移动。","尚未安装");return 1;}
        try{using(var process=Process.Start(new ProcessStartInfo(path){UseShellExecute=true,WorkingDirectory=Path.GetDirectoryName(path)})){}return 0;}
        catch(Exception e){MessageBox.Show(e.Message,"启动未完成");return 1;}
    }
}
