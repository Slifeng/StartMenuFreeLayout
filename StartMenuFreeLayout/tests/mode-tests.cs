using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
class ModeTests
{
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
    static extern bool WritePrivateProfileString(string s,string k,string v,string p);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
    static extern uint GetPrivateProfileString(string s,string k,string d,StringBuilder b,uint n,string p);
    static int Main(string[] args)
    {
        if(args[0]=="read") {
            var value=new StringBuilder(100);GetPrivateProfileString("Settings","LastMode","",value,100,args[1]);
            Console.Write(ModePolicy.Normalize(value.ToString())+":"+ModePolicy.Input(value.ToString()));return 0;
        }
        string file=Path.GetFullPath(args[0]);
        string[] modes={"free","locked","arrange","system"};int[] inputs={2,0,1,0};
        for(int i=0;i<modes.Length;i++) {
            if(!WritePrivateProfileString("Settings","LastMode",modes[i],file))throw new Exception("Save failed");
            using(var child=Process.Start(new ProcessStartInfo(Process.GetCurrentProcess().MainModule.FileName,"read \""+file+"\"") {UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true})) {
                string actual=child.StandardOutput.ReadToEnd();child.WaitForExit();
                if(actual!=modes[i]+":"+inputs[i]||child.ExitCode!=0)throw new Exception("Mode did not survive process restart: "+actual);
            }
        }
        if(ModePolicy.Normalize("corrupt")!="free")throw new Exception("Invalid mode fallback failed");
        Console.WriteLine("PASS: all four modes saved to disk and restored by fresh processes");return 0;
    }
}
