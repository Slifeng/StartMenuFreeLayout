// MIT. Per-user managed deployment; Windows Installer owns file lifecycle.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;

public static class Deployment
{
    public static RegistryKey UserRegistry=Registry.CurrentUser;
    static int InstallerSession=-1;
    static int Session {get{return InstallerSession>=0?InstallerSession:Process.GetCurrentProcess().SessionId;}}
    public static void SetInstallContext(string sid,string clientPid)
    {
        if(sid!=System.Security.Principal.WindowsIdentity.GetCurrent().User.Value)throw new Exception("安装用户身份不一致。");
        UserRegistry=Registry.Users.OpenSubKey(sid,true);
        if(UserRegistry==null)throw new Exception("无法打开安装用户的注册表。");
        using(var client=Process.GetProcessById(Int32.Parse(clientPid))) {
            if(!client.ProcessName.Equals("msiexec",StringComparison.OrdinalIgnoreCase))throw new Exception("安装客户端不匹配。");
            InstallerSession=client.SessionId;
        }
    }
    public const string Version="0.0.0";
    #if LIFECYCLE_TEST
    public const string MsiProductCode="{D6446F23-B73B-4355-A5C9-3B737225E03C}";
#else
    public const string MsiProductCode="{CB6777EC-54F7-4BD5-846A-A9645B219AD2}";
#endif
#if LIFECYCLE_TEST
    public const string Product="StartMenuFreeLayout.LifecycleTest";
    public static string Root {get{return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),Product);}}
#else
    public const string Product="StartMenuFreeLayout";
    public static string Root {get{return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),Product);}}
#endif
    public const string ProductId="StartMenuFreeLayout:9c20fae2-f1ab-4603-8ef0-45584e063f19";
    public static string Data {get{return Path.Combine(Root,"Data");}}
    public static string Config {get{return Path.Combine(Data,"layout.ini");}}
    public static string ProductKey {get{return @"Software\"+Product;}}
    public static string UninstallKey {get{return @"Software\Microsoft\Windows\CurrentVersion\Uninstall\"+Product;}}
    public static string RunKey {get{return @"Software\Microsoft\Windows\CurrentVersion\Run";}}
    public static string Marker {get{return Path.Combine(Root,".owner");}}
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    static extern bool WritePrivateProfileString(string section,string key,string value,string path);
    public static string AssemblyPath {get{return Assembly.GetExecutingAssembly().Location;}}
    public static bool Inside(string path,string root)
    {
        string p=Path.GetFullPath(path),r=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar;
        return p.StartsWith(r,StringComparison.OrdinalIgnoreCase);
    }
    static string Quote(string value){return "\""+value.Replace("\"","")+"\"";}
    static void NoLinks(string directory)
    {
        if(!Directory.Exists(directory))return;
        if((File.GetAttributes(directory)&FileAttributes.ReparsePoint)!=0)throw new Exception("拒绝清理链接目录："+directory);
        foreach(var entry in Directory.EnumerateFileSystemEntries(directory)) {
            if((File.GetAttributes(entry)&FileAttributes.ReparsePoint)!=0)throw new Exception("拒绝清理包含链接的目录："+entry);
            if(Directory.Exists(entry))NoLinks(entry);
        }
    }
    public static void ValidateOwnedRoot(string path,string token)
    {
        if(!String.Equals(Path.GetFullPath(path).TrimEnd('\\'),Path.GetFullPath(Root).TrimEnd('\\'),StringComparison.OrdinalIgnoreCase))throw new Exception("卸载目标不是本程序固定目录。");
        NoLinks(path);
        if(!File.Exists(Marker)||File.ReadAllText(Marker)!=ProductId+"\n"+token)throw new Exception("安装目录所有权核验失败，未删除文件。");
    }
    static string EnsureRoot()
    {
        if(Directory.Exists(Root)) {
            NoLinks(Root);
            if(!File.Exists(Marker)&&Directory.EnumerateFileSystemEntries(Root).Any()) {
                string app=Path.Combine(Root,"App"),version=Path.Combine(app,Version);
                string[] allowed={"StartMenuFreeLayout.exe","FreeLayout.dll","Uninstall.exe","README.md","开始使用.txt"};
                bool msiFiles=String.Equals(Path.GetDirectoryName(AssemblyPath),version,StringComparison.OrdinalIgnoreCase)
                    &&Directory.GetDirectories(Root,"*",SearchOption.AllDirectories).All(d=>String.Equals(d,app,StringComparison.OrdinalIgnoreCase)||String.Equals(d,version,StringComparison.OrdinalIgnoreCase))
                    &&Directory.GetFiles(Root,"*",SearchOption.AllDirectories).All(f=>String.Equals(Path.GetDirectoryName(f),version,StringComparison.OrdinalIgnoreCase)&&allowed.Contains(Path.GetFileName(f),StringComparer.OrdinalIgnoreCase));
                if(!msiFiles)throw new Exception("固定目录包含不属于本程序的文件，已停止安装："+Root);
            }
        }else Directory.CreateDirectory(Root);
        if(!File.Exists(Marker))File.WriteAllText(Marker,ProductId+"\n"+Guid.NewGuid().ToString("N"),new UTF8Encoding(false));
        var text=File.ReadAllText(Marker);if(!text.StartsWith(ProductId+"\n",StringComparison.Ordinal))throw new Exception("安装目录标识不匹配。");
        Directory.CreateDirectory(Data);return text.Substring(ProductId.Length+1);
    }
    public static string StartupValue()
    {
        using(var key=UserRegistry.OpenSubKey(RunKey))return key==null?"":Convert.ToString(key.GetValue(Product,""));
    }
    public static string ExecutableFromCommand(string command)
    {
        if(String.IsNullOrWhiteSpace(command))return "";
        command=command.Trim();if(command[0]=='"'){int end=command.IndexOf('"',1);return end>1?command.Substring(1,end-1):"";}
        int space=command.IndexOf(' ');return space<0?command:command.Substring(0,space);
    }
    static List<string> LegacyDirectories()
    {
        string file=Path.Combine(Data,"legacy-paths.txt");
        if(File.Exists(file))return File.ReadAllLines(file).Where(p=>!String.IsNullOrWhiteSpace(p)).ToList();
        using(var key=UserRegistry.OpenSubKey(ProductKey)) {
            if(key==null||!String.Equals(Convert.ToString(key.GetValue("InstallRoot","")),Root,StringComparison.OrdinalIgnoreCase))return new List<string>();
            return new List<string>(key.GetValue("LegacyDirectories",new string[0]) as string[]??new string[0]);
        }
    }
    static void AddLegacy(List<string> roots,string path)
    {
        if(String.IsNullOrEmpty(path))return;
        path=Path.GetFullPath(path).TrimEnd('\\');
        if(!Inside(path,Root)&&!String.Equals(path,Root,StringComparison.OrdinalIgnoreCase)&&!roots.Contains(path,StringComparer.OrdinalIgnoreCase))roots.Add(path);
    }
    public static void RegisterInstall(string importConfig)
    {
        EnsureRoot();
        var legacy=LegacyDirectories();string oldCommand=StartupValue(),oldExe=ExecutableFromCommand(oldCommand);
        if(Path.GetFileName(oldExe).Equals("StartMenuFreeLayout.exe",StringComparison.OrdinalIgnoreCase))AddLegacy(legacy,Path.GetDirectoryName(oldExe));
        if(!File.Exists(Config)) {
            var candidates=new List<string>();if(!String.IsNullOrEmpty(importConfig))candidates.Add(importConfig);
            candidates.AddRange(legacy.Select(d=>Path.Combine(d,"layout.ini")));
            string source=candidates.FirstOrDefault(File.Exists);
            if(source!=null)File.Copy(source,Config,false);
            else File.WriteAllText(Config,"[Settings]\r\nMode=inspect\r\nEditMode=2\r\nLastMode=free\r\nAutoStart=0\r\nGridRows=7\r\nLayoutVersion=2\r\n",Encoding.Unicode);
            foreach(string section in new[]{"Runtime","Startup","Testing"})WritePrivateProfileString(section,null,null,Config);
            WritePrivateProfileString("Settings","RepairFolderDuplicates","0",Config);
            WritePrivateProfileString("Settings","GridRows","7",Config);
        }
        using(var key=UserRegistry.CreateSubKey(ProductKey)) {
            key.SetValue("InstallRoot",Root);key.SetValue("CurrentExe",AssemblyPath);key.SetValue("Version",Version);
            key.SetValue("LegacyDirectories",legacy.ToArray(),RegistryValueKind.MultiString);
        }
        File.WriteAllLines(Path.Combine(Data,"legacy-paths.txt"),legacy.ToArray(),Encoding.Unicode);
        if(!String.IsNullOrEmpty(oldCommand)&&Path.GetFileName(oldExe).Equals("StartMenuFreeLayout.exe",StringComparison.OrdinalIgnoreCase))
            using(var key=UserRegistry.CreateSubKey(RunKey))key.SetValue(Product,Quote(AssemblyPath)+" /autostart");
    }
    public static string InstalledExe()
    {
        if(Inside(AssemblyPath,Path.Combine(Root,"App")))return AssemblyPath;
        using(var key=UserRegistry.OpenSubKey(ProductKey)) {
            string path=key==null?"":Convert.ToString(key.GetValue("CurrentExe",""));
            if(path.Length>0&&Inside(path,Path.Combine(Root,"App")))return path;
            return "";
        }
    }
    public static Process Start(string exe,string arguments)
    {
        return Process.Start(new ProcessStartInfo(exe,arguments){UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden});
    }
    public static void StartDetached(string exe,string arguments)
    {
        using(var process=Process.Start(new ProcessStartInfo(exe,arguments){UseShellExecute=true,WindowStyle=ProcessWindowStyle.Hidden})){}
    }
    public static void StartVisible(string exe)
    {
        using(var process=Process.Start(new ProcessStartInfo(exe){UseShellExecute=true,WindowStyle=ProcessWindowStyle.Normal,WorkingDirectory=Path.GetDirectoryName(exe)})){}
    }
    static bool OwnController(string path)
    {
        if(String.IsNullOrEmpty(path))return false;
        if(Inside(path,Path.Combine(Root,"App")))return true;
        return LegacyDirectories().Any(d=>String.Equals(path,Path.Combine(d,"StartMenuFreeLayout.exe"),StringComparison.OrdinalIgnoreCase));
    }
    public static void StopControllers()
    {
        foreach(var process in Process.GetProcessesByName("StartMenuFreeLayout").Concat(Process.GetProcessesByName("Uninstall")))using(process)try {
            if(process.Id!=Process.GetCurrentProcess().Id&&process.SessionId==Session&&OwnController(process.MainModule.FileName)){process.Kill();process.WaitForExit(5000);}
        }catch(InvalidOperationException){}
    }
    public static bool ReleaseHosts()
    {
        bool released=false;var legacy=LegacyDirectories();
        string expected=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),@"SystemApps\Microsoft.Windows.StartMenuExperienceHost_cw5n1h2txyewy\StartMenuExperienceHost.exe");
        foreach(var process in Process.GetProcessesByName("StartMenuExperienceHost"))using(process) {
            if(process.SessionId!=Session||process.HasExited||process.Threads.Count==0)continue;
            if(!String.Equals(process.MainModule.FileName,expected,StringComparison.OrdinalIgnoreCase))continue;
            bool owns=false;foreach(ProcessModule module in process.Modules)if(module.ModuleName.Equals("FreeLayout.dll",StringComparison.OrdinalIgnoreCase)) {
                owns=Inside(module.FileName,Root)||legacy.Any(d=>String.Equals(Path.GetDirectoryName(module.FileName),d,StringComparison.OrdinalIgnoreCase));
                if(owns)break;
            }
            if(owns){process.Kill();process.WaitForExit(5000);released=true;}
        }
        return released;
    }
    public static void PrepareHostUpgrade()
    {
        string old=Path.GetDirectoryName(InstalledExe()??"");
        var legacy=LegacyDirectories();bool mismatch=false;
        foreach(var p in Process.GetProcessesByName("StartMenuExperienceHost"))using(p)try {
            if(p.SessionId!=Session||p.Threads.Count==0)continue;
            foreach(ProcessModule module in p.Modules)if(module.ModuleName.Equals("FreeLayout.dll",StringComparison.OrdinalIgnoreCase)&&!String.Equals(Path.GetDirectoryName(module.FileName),old,StringComparison.OrdinalIgnoreCase)) {
                if(Inside(module.FileName,Root)||legacy.Any(d=>String.Equals(d,Path.GetDirectoryName(module.FileName),StringComparison.OrdinalIgnoreCase)))mismatch=true;
                else throw new Exception("另一份未登记的布局扩展仍在运行，请先退出那一份。");
            }
        }catch(InvalidOperationException){}
        if(mismatch){StopControllers();ReleaseHosts();Thread.Sleep(700);}
    }
    public static int Uninstall(bool silent)
    {
        if(!silent&&MessageBox.Show("卸载并删除本软件的布局、模式、日志、缓存和注册表项？\r\nWindows 已固定的应用和文件夹会保留。","卸载原生开始菜单自由布局",MessageBoxButtons.YesNo,MessageBoxIcon.Question)!=DialogResult.Yes)return 2;
        string installer=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),@"System32\msiexec.exe");
        // Windows Installer owns the file lifecycle, including the uninstaller itself.
        StartDetached(installer,"/x "+MsiProductCode+(silent?" /qn":" /passive")+" /norestart");
        return 0;
    }
    public static void PrepareUninstall()
    {
        if(!Directory.Exists(Root))return;
        string marker=File.ReadAllText(Marker),prefix=ProductId+"\n";
        if(!marker.StartsWith(prefix,StringComparison.Ordinal))throw new Exception("安装标识无效。");
        string token=marker.Substring(prefix.Length);ValidateOwnedRoot(Root,token);
        Program.SetForUninstall();
        using(var run=UserRegistry.OpenSubKey(RunKey,true))if(run!=null&&OwnController(ExecutableFromCommand(Convert.ToString(run.GetValue(Product,"")))))run.DeleteValue(Product,false);
        StopControllers();Thread.Sleep(400);ReleaseHosts();Thread.Sleep(400);
        ValidateOwnedRoot(Root,token);
        if(Directory.Exists(Data))Directory.Delete(Data,true);
        File.Delete(Marker);
        UserRegistry.DeleteSubKeyTree(UninstallKey,false);
        UserRegistry.DeleteSubKeyTree(ProductKey,false);
        if(StartupValue().Length>0)throw new Exception("本软件自启动项尚未清理。");
#if LIFECYCLE_TEST
        using(var check=UserRegistry.OpenSubKey(ProductKey))File.WriteAllText(Path.Combine(Path.GetTempPath(),Product+".audit"),"registry="+UserRegistry.Name+" exists="+(check!=null)+" startup="+StartupValue().Length+" identity="+System.Security.Principal.WindowsIdentity.GetCurrent().Name+" session="+Session);
#endif
        // Only MSI-tracked binaries remain. RemoveFiles/RemoveFolders remove them
        // after this process exits, without a script or a temporary executable.
    }
}
