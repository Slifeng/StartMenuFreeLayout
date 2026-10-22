using System;
internal static class ModePolicy
{
    public static string Normalize(string value)
    {return value=="free"||value=="locked"||value=="arrange"||value=="system"?value:"free";}
    public static int Input(string mode)
    {mode=Normalize(mode);return mode=="arrange"?1:(mode=="free"?2:0);}
    public static string Name(string mode)
    {
        switch(Normalize(mode)) {
            case "free":return "自由拖动并单击打开";
            case "locked":return "Ctrl 拖动并单击打开";
            case "arrange":return "只整理图标，暂停启动";
            default:return "系统原始布局";
        }
    }
}
