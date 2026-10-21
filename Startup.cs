// Per-user, optional logon startup. No administrator rights or system services.
using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;

internal static partial class Program
{
    const string StartupKey=@"Software\Microsoft\Windows\CurrentVersion\Run";
    const string StartupName=Deployment.Product;
    static string StartupCommand {get{return "\""+Application.ExecutablePath+"\" /autostart";}}
    static string LastMode()
    {
        string stored=Read("Settings","LastMode","");
        if(stored.Length>0)return ModePolicy.Normalize(stored);
        if(Read("Settings","Mode","inspect")!="enable")return "system";
        return Read("Settings","EditMode","0")=="1"?"arrange":"locked";
    }
    static bool StartupEnabled()
    {
        using(var key=Deployment.UserRegistry.OpenSubKey(StartupKey))
            return key!=null&&String.Equals(Convert.ToString(key.GetValue(StartupName)),StartupCommand,StringComparison.OrdinalIgnoreCase);
    }
    static void StartupLog(string message)
    {
        try {File.AppendAllText(Path.Combine(Deployment.Data,"startup.log"),DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss ")+message+Environment.NewLine,Encoding.UTF8);}catch{}
    }
    static void SetStartup(bool enabled)
    {
        using(var key=Deployment.UserRegistry.CreateSubKey(StartupKey)) {
            if(enabled)key.SetValue(StartupName,StartupCommand,RegistryValueKind.String);
            else if(String.Equals(Convert.ToString(key.GetValue(StartupName)),StartupCommand,StringComparison.OrdinalIgnoreCase))key.DeleteValue(StartupName,false);
        }
        Write("Settings","AutoStart",enabled?"1":"0");
        StartupLog(enabled?"Autostart enabled":"Autostart disabled");
        if(enabled)Deployment.StartDetached(Application.ExecutablePath,"/autostart");
    }
    static string ApplyMode(string mode,bool remember)
    {
        mode=ModePolicy.Normalize(mode);
        if(mode=="system") {
            Write("Settings","Mode","inspect");
            if(remember)Write("Settings","LastMode",mode);
            return "已还原系统布局；下次登录仍保留系统布局。";
        }
#if LIFECYCLE_TEST
        Write("Settings","EditMode",ModePolicy.Input(mode).ToString());Write("Settings","Mode","enable");if(remember)Write("Settings","LastMode",mode);return "test-mode:"+mode;
#else
        Deployment.PrepareHostUpgrade();
        Process ready=null;for(int i=0;i<30&&ready==null;i++){try{ready=GetTarget();}catch{Thread.Sleep(300);}}
        if(ready==null)throw new Exception("开始菜单正在重启，请稍后重试。");
        using(var target=ready) {
            string version=Validate(target);
            Write("Settings","EditMode",ModePolicy.Input(mode).ToString());
            Write("Runtime","Status","attaching");Write("Settings","Mode","enable");
            try {Attach(target);}catch{Write("Settings","Mode","inspect");throw;}
            if(remember)Write("Settings","LastMode",mode);
            return version+"\r\n当前："+ModePolicy.Name(mode)+"。\r\n模式和格子位置已保存，下次登录按此恢复。";
        }
#endif
    }
    static int RunAutoStart()
    {
        bool created;
        using(var mutex=new Mutex(true,@"Local\"+Deployment.Product+".AutoStart",out created)) {
            if(!created)return 0;
            try {
                if(!StartupEnabled())return 0;
                var context=new ApplicationContext();
                var timer=new System.Windows.Forms.Timer {Interval=2000};
                int appliedPid=-1,attemptPid=-1,attempts=0;string appliedMode="",attemptMode="";
                DateTime retryAt=DateTime.MinValue;
                timer.Tick+=(sender,args)=> {
                    if(!StartupEnabled()){timer.Stop();context.ExitThread();return;}
                    string mode=LastMode();
                    try {
                        using(var target=GetTarget()) {
                            if(target.Id==appliedPid&&mode==appliedMode)return;
                            if(target.Id!=attemptPid||mode!=attemptMode){attemptPid=target.Id;attemptMode=mode;attempts=0;retryAt=DateTime.MinValue;}
                            if(attempts>=3||DateTime.UtcNow<retryAt)return;
                            attempts++;
                            try {
                                ApplyMode(mode,false);appliedPid=target.Id;appliedMode=mode;
                                Write("Startup","Status","watching");Write("Startup","RestoredMode",mode);Write("Startup","HostPid",target.Id.ToString());Write("Startup","Error","");
                                StartupLog("Restored mode="+mode+" host="+target.Id);
                            }catch(Exception ex){
                                retryAt=DateTime.UtcNow.AddSeconds(attempts*10);
                                Write("Startup","Status","retrying");Write("Startup","Error",ex.Message);
                                StartupLog("Restore attempt "+attempts+": "+ex.Message);
                                if(attempts>=3){Write("Startup","Status","failed");Write("Runtime","Status","startup-failed");}
                            }
                        }
                    }catch(Exception ex) {
                        // At logon the shell process may not exist yet. Keep
                        // waiting silently; never start or elevate a shell.
                        Write("Startup","Status","waiting-host");
                        Write("Runtime","Status","waiting-host");
                        Write("Startup","Error",ex.Message);
                    }
                };
                Write("Startup","Status","waiting-host");StartupLog("Background restorer started");
                timer.Start();Application.Run(context);timer.Dispose();StartupLog("Background restorer stopped");
                return 0;
            }finally{mutex.ReleaseMutex();}
        }
    }
}
