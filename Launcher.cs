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
    static readonly string Ini=Path.Combine(Dir,"layout.ini");
    const string ExpectedVersion="10.0.26100.7309";
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
            string result="Windows 11 "+build+"."+ubr+" / 开始菜单 "+version+" / x64";
            if(IntPtr.Size!=8 || build!="26200" || ubr!=7840 || version!=ExpectedVersion)
                throw new Exception("当前版本不在此实验版本的适配范围，已停止加载。\r\n"+result+"\r\n目标：26200.7840 / "+ExpectedVersion);
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
        if(!File.Exists(Ini))File.WriteAllText(Ini,"[Settings]\r\nMode=inspect\r\nCanvasHeight=440\r\n",Encoding.ASCII);
    }
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
        CoInitializeEx(IntPtr.Zero,2);
        if(args.Length>0)
        {
            try {string action=args[0].TrimStart('/','-').ToLowerInvariant();if(action=="autostart")return RunAutoStart();Console.WriteLine(Run(action));return 0;}
            catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}
        }
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        var form=new Form {Text="原生开始菜单 · 网格布局 v3.3.2（实验版）",Width=640,Height=500,StartPosition=FormStartPosition.CenterScreen,FormBorderStyle=FormBorderStyle.FixedDialog,MaximizeBox=false};
        var info=new Label {Left=24,Top=20,Width=575,Height=70,Text="适配 Windows 11 25H2 26200.7840\r\n图标吸附到格子，允许留空、不自动补位。\r\n应用重叠建文件夹；仅剩一个成员时自动解散，成员留在原格子。"};
        form.Controls.Add(info);
        var output=new TextBox {Left=24,Top=218,Width=575,Height=149,Multiline=true,ReadOnly=true,ScrollBars=ScrollBars.Vertical};
        form.Controls.Add(output);
        form.Controls.Add(new Label {Left=24,Top=153,Width=575,Height=30,Text="自由模式：单击打开，直接拖动。锁定模式：单击打开，Ctrl + 拖动。"});
        bool syncingStartup=false;
        var startup=new CheckBox {Left=24,Top=184,Width=575,Text="开机自启动：登录后静默恢复上次模式和图标位置",Checked=StartupEnabled()};
        startup.CheckedChanged+=(sender,eventArgs)=> {
            if(syncingStartup)return;
            try{SetStartup(startup.Checked);output.Text=startup.Checked?"自启动已开启；每次切换模式都会立即保存，下次登录后自动恢复。":"自启动已关闭，当前模式继续生效。";}
            catch(Exception ex){syncingStartup=true;startup.Checked=StartupEnabled();syncingStartup=false;output.Text=ex.Message;}
        };form.Controls.Add(startup);
        string[] texts={"自由拖动＋点击","锁定位置使用","只整理图标","还原系统布局"};string[] actions={"free","locked","edit","restore"};
        for(int i=0;i<texts.Length;i++)
        {
            string action=actions[i];var button=new Button {Left=24+i*146,Top=106,Width=139,Height=38,Text=texts[i]};
            button.Click+=(s,e)=> {try {output.Text=Run(action);}catch(Exception ex){output.Text=ex.Message;}};form.Controls.Add(button);
        }
        var status=new Label {Left=24,Top=384,Width=575,Height=55};form.Controls.Add(status);
        var timer=new System.Windows.Forms.Timer {Interval=800};timer.Tick+=(s,e)=> {
            string value=Read("Runtime","Status","未加载");
            switch(value) {
                case "active":value="网格布局已启用 · 点击启动应用";break;
                case "active-free":value="自由模式 · 直接拖动，单击打开";break;
                case "active-edit":value="网格摆放模式 · 拖动后吸附到格子";break;
                case "inspected":value="已连接 · 使用系统网格布局";break;
                case "restored":value="已还原系统布局";break;
                case "attaching":value="正在连接开始菜单";break;
                case "waiting-host":value="开始菜单尚未就绪，后台正在等待恢复";break;
                case "startup-failed":value="自动恢复失败，请查看 startup.log";break;
                case "waiting-containers":value="请打开一次开始菜单";break;
                case "error-restored":value="发生错误，已请求还原";break;
                case "restore-failed":value="还原未完成，请重启电脑恢复";break;
                case "save-failed":value="位置保存失败，请检查文件夹写入权限";break;
                case "diagnostics-failed":value="未能连接开始菜单";break;
            }
            string grid=Read("Runtime","Grid","");status.Text=value+(grid.Length>0?"　"+grid.Replace("x"," × "):"")+"\r\n已记住："+ModePolicy.Name(LastMode());
            bool enabled=StartupEnabled();if(startup.Checked!=enabled){syncingStartup=true;startup.Checked=enabled;syncingStartup=false;}
        };timer.Start();
        Application.Run(form);return 0;
    }
}
