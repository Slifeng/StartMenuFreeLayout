// MIT license. The settings window does not replace the Windows Start menu.
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;

internal static partial class Program
{
    static readonly string Dir=AppDomain.CurrentDomain.BaseDirectory;
    static readonly string Ini=Deployment.Config;
    const string ExpectedPlatformFingerprint="D400AD6538382C747BB5170D13B0C7613255BF2B61608896AC7B9F05D07D3CDB";
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    static extern IntPtr LoadLibraryEx(string file,IntPtr reserved,uint flags);
    [DllImport("kernel32.dll",CharSet=CharSet.Ansi,SetLastError=true)]
    static extern IntPtr GetProcAddress(IntPtr module,string name);
    [DllImport("ole32.dll")] static extern int CoInitializeEx(IntPtr reserved,uint mode);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    static extern bool WritePrivateProfileString(string section,string key,string value,string path);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
    static extern uint GetPrivateProfileString(string section,string key,string fallback,StringBuilder result,uint size,string path);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    delegate int NativeAttach(uint processId);
    static string Read(string section,string key,string fallback)
    { var s=new StringBuilder(2048);GetPrivateProfileString(section,key,fallback,s,2048,Ini);return s.ToString(); }
    static void Write(string section,string key,string value)
    { if(!WritePrivateProfileString(section,key,value,Ini))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error()); }
    static string HostVersion(Process p)
    { var v=FileVersionInfo.GetVersionInfo(p.MainModule.FileName);return String.Format("{0}.{1}.{2}.{3}",v.FileMajorPart,v.FileMinorPart,v.FileBuildPart,v.FilePrivatePart); }
    static Process GetTarget()
    {
        int session=Process.GetCurrentProcess().SessionId;
        string expected=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),@"SystemApps\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\StartMenuExperienceHost.exe");
        var targets=new System.Collections.Generic.List<Process>();
        foreach(var candidate in Process.GetProcessesByName("StartMenuExperienceHost")) {
            bool usable=false;
            try{usable=candidate.SessionId==session&&!candidate.HasExited&&candidate.Threads.Count>0&&String.Equals(candidate.MainModule.FileName,expected,StringComparison.OrdinalIgnoreCase);}catch{}
            if(usable)targets.Add(candidate);else candidate.Dispose();
        }
        if(targets.Count==0)throw new Exception("开始菜单尚未就绪，正在等待可运行的菜单进程。");
        // WER can retain a reflected, zero-thread clone after a crash. During
        // replacement there may also briefly be two live hosts: prefer newest.
        var target=targets.OrderByDescending(p=>p.StartTime).First();
        foreach(var other in targets)if(other!=target)other.Dispose();
        return target;
    }
    static string Validate(Process p)
    {
        using(var k=Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Microsoft\Windows NT\CurrentVersion"))
        {
            string build=Convert.ToString(k.GetValue("CurrentBuild"));int ubr=Convert.ToInt32(k.GetValue("UBR"));
            string version=HostVersion(p);
            string result="Windows 原生开始菜单 / x64";
            string platform=build+"|"+ubr.ToString(System.Globalization.CultureInfo.InvariantCulture)+"|"+version;
            string fingerprint;
            using(var sha=System.Security.Cryptography.SHA256.Create())fingerprint=BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(platform))).Replace("-","");
            if(IntPtr.Size!=8 || fingerprint!=ExpectedPlatformFingerprint)
                throw new Exception("当前系统或开始菜单组件不在此实验版本的适配范围，已停止加载。");
            string expected=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),@"SystemApps\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\StartMenuExperienceHost.exe");
            if(!String.Equals(p.MainModule.FileName,expected,StringComparison.OrdinalIgnoreCase))
                throw new Exception("开始菜单进程路径与系统组件路径不符，已停止加载。");
            string core=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),@"SystemApps\MicrosoftWindows.Client.Core_cw5n1h2txyewy\StartMenu.dll");
            using(var stream=File.OpenRead(core))using(var sha=System.Security.Cryptography.SHA256.Create()) {
                string hash=BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","");
                if(hash!="B0A00A39F9160941B1B9B9327D74A890EA7639BFA0BA282E0BBD503648B41433")
                    throw new Exception("开始菜单组件与原生文件夹接口适配版本不匹配，已停止加载。");
            }
            return result;
        }
    }
    static void EnsureSettings()
    {
        Directory.CreateDirectory(Deployment.Data);
        if(!File.Exists(Ini))File.WriteAllText(Ini,"[Settings]\r\nMode=inspect\r\nEditMode=2\r\nLastMode=system\r\nGridRows=7\r\n",Encoding.Unicode);
    }
    public static void SetForUninstall(){Write("Settings","Mode","inspect");Write("Settings","AutoStart","0");Write("Settings","LastMode","system");}
    static bool AlreadyLoaded(Process p)
    {
        string path=Path.Combine(Dir,"FreeLayout.dll");
        foreach(ProcessModule module in p.Modules)
        {
            if(String.Equals(module.FileName,path,StringComparison.OrdinalIgnoreCase))return true;
            if(String.Equals(module.ModuleName,"FreeLayout.dll",StringComparison.OrdinalIgnoreCase))
                throw new Exception("开始菜单已从其他文件夹加载此扩展。请先用原目录中的控制程序还原，并重启开始菜单或电脑，再换目录启用。\r\n已加载位置："+module.FileName);
        }
        return false;
    }
    static void Attach(Process p)
    {
        string dll=Path.Combine(Dir,"FreeLayout.dll");if(!File.Exists(dll))throw new FileNotFoundException("缺少 FreeLayout.dll，需先完成构建。",dll);
        if(AlreadyLoaded(p))return;
        var module=LoadLibraryEx(dll,IntPtr.Zero,0x100|0x800);
        if(module==IntPtr.Zero)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        var ptr=GetProcAddress(module,"AttachToStart");if(ptr==IntPtr.Zero)throw new Exception("扩展缺少 AttachToStart 入口。");
        var attach=(NativeAttach)Marshal.GetDelegateForFunctionPointer(ptr,typeof(NativeAttach));
        int hr=attach((uint)p.Id);
        if(hr<0)Marshal.ThrowExceptionForHR(hr);
        for(int i=0;i<25;i++){Thread.Sleep(200);if(Read("Runtime","Status","")!="attaching")return;}
        throw new Exception("未收到扩展接入回报。布局尚未验证，请查看 extension.log。");
    }
    static string Run(string action)
    {
        EnsureSettings();
        if(action=="startup-on"){SetStartup(true);return "已开启登录后自启动，静默恢复上次模式。";}
        if(action=="startup-off"){SetStartup(false);return "已关闭自启动；当前布局继续生效。";}
        if(action=="resume")return ApplyMode(LastMode(),false);
        if(action=="saved-mode")return LastMode();
        if(action=="enable"||action=="free")return ApplyMode("free",true);
        if(action=="locked"||action=="use")return ApplyMode("locked",true);
        if(action=="edit")return ApplyMode("arrange",true);
        if(action=="restore")return ApplyMode("system",true);
        if(action=="pause"){Write("Settings","Mode","inspect");return "已暂停；记忆模式不变。";}
        using(var target=GetTarget())
        {
            string version=Validate(target);
            if(action=="diagnose")return version+"\r\n版本校验通过；此操作没有加载扩展。";
            if(action=="status")return version+"\r\n状态："+Read("Runtime","Status","未加载");
            if(action!="inspect")throw new Exception("未知操作。支持 /diagnose /inspect /free /locked /edit /restore /resume /startup-on /startup-off /status。");
            Write("Runtime","Status","attaching");Write("Settings","Mode","inspect");Write("Settings","LastMode","system");
            try {Attach(target);}
            catch {Write("Settings","Mode","inspect");throw;}
            return version+"\r\n当前为只读检查模式，不修改图标布局。";
        }
    }
    [STAThread]
    static int Main(string[] args)
    {
        try {
#if UNINSTALLER
            return Deployment.Uninstall(args.Any(a=>a.Equals("/uninstall-silent",StringComparison.OrdinalIgnoreCase)));
#else
            if(args.Length>0&&(args[0]=="/uninstall"||args[0]=="/uninstall-silent"))return Deployment.Uninstall(args[0]=="/uninstall-silent");
            if(args.Length>0&&args[0]=="/register-install"){Deployment.SetInstallContext(args[2],args[3]);Deployment.RegisterInstall(args[1]);return 0;}
            if(args.Length>0&&args[0]=="/prepare-uninstall"){Deployment.SetInstallContext(args[1],args[2]);Deployment.PrepareUninstall();return 0;}
            if(args.Length>0&&args[0]=="/launch-control"){Deployment.SetInstallContext(args[1],args[2]);Deployment.StartVisible(Deployment.AssemblyPath);return 0;}
#if LIFECYCLE_TEST
            if(args.Length>0&&args[0]=="/lifecycle-selftest") {
                Deployment.SetInstallContext(args[1],args[2]);
                Run("free");if(Run("saved-mode")!="free")throw new Exception("Saved mode mismatch");
                Run("startup-on");
                if(!Deployment.StartupValue().StartsWith("\""+Application.ExecutablePath+"\""))throw new Exception("Startup path mismatch");
                File.WriteAllText(Path.Combine(Deployment.Data,"selftest.ok"),"mode=free;startup=stable;root="+Deployment.Root);
                return 0;
            }
#endif
#endif
        }catch(Exception e){Console.Error.WriteLine(e.Message);if(args.Length==0)MessageBox.Show(e.Message,"操作未完成");return 1;}
#if !UNINSTALLER
        CoInitializeEx(IntPtr.Zero,2);
        if(args.Length>0)
        {
            try {string action=args[0].TrimStart('/','-').ToLowerInvariant();if(action=="autostart")return RunAutoStart();Console.WriteLine(Run(action));return 0;}
            catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}
        }
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        EnsureSettings();
        if(StartupEnabled())Deployment.StartDetached(Application.ExecutablePath,"/autostart");
        return ShowControlWindow();
#endif
    }
}
