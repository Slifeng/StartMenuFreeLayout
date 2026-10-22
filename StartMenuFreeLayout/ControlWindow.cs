// MIT. Native WinForms presentation; mode, startup and deployment stay shared.
#if !UNINSTALLER
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

internal static partial class Program
{
    [DllImport("ole32.dll")] static extern void CoUninitialize();
    static int ShowControlWindow()
    {
        using(var window=new LayoutControlWindow(CaptureUiState,RunUiAction,delegate {return Deployment.Uninstall(false);}))
            Application.Run(window);
        return 0;
    }
    static string RunUiAction(string action)
    {
        int initialized=CoInitializeEx(IntPtr.Zero,2);
        try {return Run(action);}
        finally {if(initialized>=0)CoUninitialize();}
    }
    static LayoutUiState CaptureUiState()
    {
        return new LayoutUiState {
            Mode=LastMode(), Status=Read("Runtime","Status","未加载"),
            Grid=Read("Runtime","Grid",""), AutoStart=StartupEnabled(),
            GridReady=Read("Runtime","GridCheck","")=="pass",
            StartupError=Read("Startup","Error","")
        };
    }
}

internal sealed class LayoutUiState
{
    public string Mode,Status,Grid,StartupError;
    public bool AutoStart,GridReady;
    public bool Connected {get {return Status=="active"||Status.StartsWith("active-")||Status=="waiting-containers"||Status=="inspected"||Status=="restored";}}
    public bool Failed {get {return Status.Contains("failed")||Status=="error-restored";}}
    public string StatusText {
        get {
            switch(Status) {
                case "active":case "active-free":case "active-edit":return "已连接开始菜单";
                case "restored":return "使用系统原生布局";
                case "inspected":return "已连接 · 系统布局";
                case "attaching":return "正在连接开始菜单";
                case "waiting-host":return "等待开始菜单就绪";
                case "waiting-containers":return "请打开一次开始菜单";
                case "startup-failed":return "自动恢复失败";
                case "error-restored":return "发生错误，已请求还原";
                case "restore-failed":return "还原未完成，请查看运行信息";
                case "save-failed":return "布局保存失败";
                case "diagnostics-failed":return "未能连接开始菜单";
                default:return "尚未连接开始菜单";
            }
        }
    }
    public string GridText {
        get {
            if(Mode=="system")return "系统默认排列";
            string[] parts=(Grid??"").Split('x');int columns,rows;
            if(parts.Length==2&&Int32.TryParse(parts[0],out columns)&&Int32.TryParse(parts[1],out rows))
                return columns+" 列 × "+rows+" 行";
            return "等待布局信息";
        }
    }
}

internal static class LayoutVisuals
{
    public static readonly Color Blue=Color.FromArgb(0,112,245),Ink=Color.FromArgb(27,35,48),Muted=Color.FromArgb(112,124,143),Line=Color.FromArgb(221,227,235);
    public static GraphicsPath Round(RectangleF r,float radius)
    {
        float d=radius*2;var p=new GraphicsPath();
        p.AddArc(r.Left,r.Top,d,d,180,90);p.AddArc(r.Right-d,r.Top,d,d,270,90);
        p.AddArc(r.Right-d,r.Bottom-d,d,d,0,90);p.AddArc(r.Left,r.Bottom-d,d,d,90,90);p.CloseFigure();return p;
    }
    public static void Box(Graphics g,RectangleF r,Color fill,Color border,float radius)
    {
        using(var p=Round(r,radius)) {using(var b=new SolidBrush(fill))g.FillPath(b,p);using(var pen=new Pen(border,1))g.DrawPath(pen,p);}
    }
    public static void Text(Graphics g,string text,RectangleF bounds,float size,Color color,bool bold,bool center)
    {
        using(var f=new Font("Microsoft YaHei UI",size,bold?FontStyle.Bold:FontStyle.Regular,GraphicsUnit.Pixel))
        using(var b=new SolidBrush(color))using(var format=new StringFormat()) {
            format.Alignment=center?StringAlignment.Center:StringAlignment.Near;format.LineAlignment=StringAlignment.Center;
            format.Trimming=StringTrimming.EllipsisCharacter;format.FormatFlags=StringFormatFlags.NoWrap;
            g.DrawString(text,f,b,bounds,format);
        }
    }
    public static float Begin(Graphics g,Control owner)
    {
        var window=owner.FindForm() as LayoutControlWindow;float scale=window==null?1:window.RenderScale;g.PageUnit=GraphicsUnit.Pixel;g.ScaleTransform(scale,scale);g.SmoothingMode=SmoothingMode.AntiAlias;
        g.TextRenderingHint=System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;return scale;
    }
    public static void Mark(Graphics g,float x,float y,float size,Color color)
    {
        float cell=size/3.5f,gap=size/14f;
        using(var b=new SolidBrush(color))for(int r=0;r<3;r++)for(int c=0;c<3;c++) {
            if(r==2&&c==2)continue;
            using(var p=Round(new RectangleF(x+c*(cell+gap),y+r*(cell+gap),cell,cell),2))g.FillPath(b,p);
        }
    }
    public static void Symbol(Graphics g,string kind,float x,float y,float size,Color color)
    {
        var state=g.Save();g.TranslateTransform(x,y);g.ScaleTransform(size/48,size/48);
        using(var pen=new Pen(color,2.4f)) {
            pen.StartCap=LineCap.Round;pen.EndCap=LineCap.Round;pen.LineJoin=LineJoin.Round;
            if(kind=="free") {
                g.DrawLine(pen,24,3,24,45);g.DrawLine(pen,3,24,45,24);
                g.DrawLines(pen,new[]{new PointF(17,10),new PointF(24,3),new PointF(31,10)});
                g.DrawLines(pen,new[]{new PointF(17,38),new PointF(24,45),new PointF(31,38)});
                g.DrawLines(pen,new[]{new PointF(10,17),new PointF(3,24),new PointF(10,31)});
                g.DrawLines(pen,new[]{new PointF(38,17),new PointF(45,24),new PointF(38,31)});
            } else if(kind=="locked") {
                g.DrawArc(pen,13,3,22,28,180,180);
                using(var p=Round(new RectangleF(8,20,32,25),4))g.DrawPath(pen,p);
                using(var b=new SolidBrush(color))g.FillEllipse(b,22,30,4,6);
            } else if(kind=="arrange") {
                using(var p=Round(new RectangleF(3,4,16,16),2))g.DrawPath(pen,p);
                using(var p=Round(new RectangleF(25,4,16,16),2))g.DrawPath(pen,p);
                using(var p=Round(new RectangleF(3,27,16,16),2))g.DrawPath(pen,p);
                g.DrawPolygon(pen,new[]{new PointF(24,37),new PointF(39,22),new PointF(45,28),new PointF(30,43),new PointF(23,45)});
                g.DrawLine(pen,36,25,42,31);
            } else if(kind=="system") {
                g.DrawLines(pen,new[]{new PointF(18,7),new PointF(6,19),new PointF(18,31)});
                g.DrawLine(pen,6,19,30,19);g.DrawArc(pen,18,19,26,25,270,180);g.DrawLine(pen,31,44,20,44);
            } else if(kind=="power") {
                g.DrawLine(pen,24,3,24,23);g.DrawArc(pen,6,7,36,36,-55,290);
            } else if(kind=="grid") {
                foreach(var r in new[]{new RectangleF(7,7,12,12),new RectangleF(28,7,12,12),new RectangleF(7,28,12,12),new RectangleF(28,28,12,12)})
                    using(var p=Round(r,1.5f))g.DrawPath(pen,p);
            } else if(kind=="check") {
                g.DrawEllipse(pen,4,4,40,40);g.DrawLines(pen,new[]{new PointF(14,24),new PointF(21,31),new PointF(34,17)});
            } else if(kind=="trash") {
                g.DrawLine(pen,9,12,39,12);g.DrawLine(pen,18,6,30,6);g.DrawLine(pen,14,13,16,42);g.DrawLine(pen,34,13,32,42);g.DrawLine(pen,16,42,32,42);g.DrawLine(pen,22,20,22,35);g.DrawLine(pen,28,20,28,35);
            }
        }g.Restore(state);
    }
    public static Icon AppIcon()
    {
        using(var stream=typeof(LayoutVisuals).Assembly.GetManifestResourceStream("StartMenuFreeLayout.AppIcon.ico")) {
            if(stream==null)throw new InvalidOperationException("缺少应用图标资源。");
            using(var icon=new Icon(stream,new Size(256,256)))return (Icon)icon.Clone();
        }
    }
}

internal class LayoutPanel:Panel
{
    public LayoutPanel() {DoubleBuffered=true;BackColor=Color.White;}
    protected override void OnPaint(PaintEventArgs e)
    {
        var saved=e.Graphics.Save();float s=LayoutVisuals.Begin(e.Graphics,this);
        LayoutVisuals.Box(e.Graphics,new RectangleF(.6f,.6f,Width/s-1.2f,Height/s-1.2f),Color.White,LayoutVisuals.Line,9);
        e.Graphics.Restore(saved);base.OnPaint(e);
    }
}

internal class LayoutModeCard:Button
{
    public readonly string Mode;public string Caption,Description;bool chosen,hover;
    public bool Chosen {get {return chosen;}set {if(chosen==value)return;chosen=value;Invalidate();AccessibilityNotifyClients(AccessibleEvents.StateChange,-1);}}
    public LayoutModeCard(string mode,string caption,string description)
    {
        Mode=mode;Caption=caption;Description=description;Text=caption;AccessibleName=caption;AccessibleDescription=description;
        AccessibleRole=AccessibleRole.RadioButton;Cursor=Cursors.Hand;FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;
        SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);
        BackColor=Color.White;UseVisualStyleBackColor=false;
    }
    protected override AccessibleObject CreateAccessibilityInstance(){return new CardAccessibility(this);}
    sealed class CardAccessibility:Control.ControlAccessibleObject
    {
        readonly LayoutModeCard card;
        public CardAccessibility(LayoutModeCard owner):base(owner){card=owner;}
        public override AccessibleStates State {get{return base.State|(card.Chosen?AccessibleStates.Checked:AccessibleStates.None);}}
        public override string DefaultAction {get{return "选择模式";}}
        public override void DoDefaultAction(){if(card.Enabled)card.PerformClick();}
    }
    protected override void OnPaintBackground(PaintEventArgs e){e.Graphics.Clear(BackColor);}
    protected override void OnMouseEnter(EventArgs e){hover=true;Invalidate();base.OnMouseEnter(e);}
    protected override void OnMouseLeave(EventArgs e){hover=false;Invalidate();base.OnMouseLeave(e);}
    protected override void OnGotFocus(EventArgs e){base.OnGotFocus(e);Invalidate();}
    protected override void OnLostFocus(EventArgs e){base.OnLostFocus(e);Invalidate();}
    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(BackColor);float s=LayoutVisuals.Begin(e.Graphics,this),w=Width/s,h=Height/s;
        Color border=Chosen?LayoutVisuals.Blue:(hover?Color.FromArgb(166,187,214):LayoutVisuals.Line);
        Color fill=Chosen?Color.FromArgb(235,244,255):(hover?Color.FromArgb(247,250,254):Color.White);
        LayoutVisuals.Box(e.Graphics,new RectangleF(1,1,w-2,h-2),fill,border,9);
        Color ink=Enabled?LayoutVisuals.Ink:LayoutVisuals.Muted;
        LayoutVisuals.Symbol(e.Graphics,Mode,w/2-22,20,44,Chosen?LayoutVisuals.Blue:ink);
        LayoutVisuals.Text(e.Graphics,Caption,new RectangleF(15,76,w-30,27),16,ink,true,true);
        LayoutVisuals.Text(e.Graphics,Description,new RectangleF(15,105,w-30,22),12.5f,LayoutVisuals.Muted,false,true);
        if(Chosen) {
            using(var b=new SolidBrush(LayoutVisuals.Blue))e.Graphics.FillEllipse(b,w-36,12,22,22);
            using(var pen=new Pen(Color.White,1.9f)){pen.StartCap=pen.EndCap=LineCap.Round;e.Graphics.DrawLines(pen,new[]{new PointF(w-30,23),new PointF(w-26,27),new PointF(w-20,20)});}
        }
        if(Focused&&ShowFocusCues)using(var p=LayoutVisuals.Round(new RectangleF(4,4,w-8,h-8),7))using(var pen=new Pen(LayoutVisuals.Blue,1)){pen.DashStyle=DashStyle.Dot;e.Graphics.DrawPath(pen,p);}
    }
}

internal sealed class LayoutToggle:CheckBox
{
    public LayoutToggle()
    {
        AutoSize=false;Text="开机自启动";AccessibleName=Text;Cursor=Cursors.Hand;BackColor=Color.White;
        SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);
    }
    protected override void OnPaintBackground(PaintEventArgs e){e.Graphics.Clear(BackColor);}
    protected override void OnCheckedChanged(EventArgs e){base.OnCheckedChanged(e);Invalidate();}
    protected override void OnGotFocus(EventArgs e){base.OnGotFocus(e);Invalidate();}
    protected override void OnLostFocus(EventArgs e){base.OnLostFocus(e);Invalidate();}
    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(BackColor);float s=LayoutVisuals.Begin(e.Graphics,this),h=Height/s,w=Width/s;
        Color fill=Checked?LayoutVisuals.Blue:Color.FromArgb(179,188,202);if(!Enabled)fill=Color.FromArgb(180,190,203);
        LayoutVisuals.Box(e.Graphics,new RectangleF(2,h/2-13,47,26),fill,fill,13);
        using(var b=new SolidBrush(Color.White))e.Graphics.FillEllipse(b,Checked?26:5,h/2-10,20,20);
        LayoutVisuals.Text(e.Graphics,Checked?"开启":"关闭",new RectangleF(60,0,w-60,h),14,LayoutVisuals.Ink,false,false);
        if(Focused&&ShowFocusCues)using(var pen=new Pen(LayoutVisuals.Blue,1)){pen.DashStyle=DashStyle.Dot;e.Graphics.DrawRectangle(pen,0,2,w-2,h-4);}
    }
}

internal sealed class LayoutActionButton:Button
{
    public bool Disclosure,Expanded,Danger;bool hover;
    public LayoutActionButton(){FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;Cursor=Cursors.Hand;BackColor=Color.White;UseVisualStyleBackColor=false;SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);}
    protected override void OnPaintBackground(PaintEventArgs e){e.Graphics.Clear(BackColor);}
    protected override void OnMouseEnter(EventArgs e){hover=true;Invalidate();base.OnMouseEnter(e);}
    protected override void OnMouseLeave(EventArgs e){hover=false;Invalidate();base.OnMouseLeave(e);}
    protected override void OnGotFocus(EventArgs e){base.OnGotFocus(e);Invalidate();}
    protected override void OnLostFocus(EventArgs e){base.OnLostFocus(e);Invalidate();}
    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(BackColor);float s=LayoutVisuals.Begin(e.Graphics,this),w=Width/s,h=Height/s;Color ink=Danger?Color.FromArgb(233,65,52):LayoutVisuals.Ink;
        Color fill=hover?(Danger?Color.FromArgb(255,245,244):Color.FromArgb(247,250,253)):Color.White;
        LayoutVisuals.Box(e.Graphics,new RectangleF(1,1,w-2,h-2),fill,Disclosure?LayoutVisuals.Line:fill,7);
        if(Danger)LayoutVisuals.Symbol(e.Graphics,"trash",0,h/2-11,22,ink);
        else using(var pen=new Pen(ink,1.8f)) {
            float y=h/2;var points=Expanded?new[]{new PointF(14,y+2),new PointF(19,y-3),new PointF(24,y+2)}:new[]{new PointF(14,y-2),new PointF(19,y+3),new PointF(24,y-2)};
            e.Graphics.DrawLines(pen,points);
        }
        LayoutVisuals.Text(e.Graphics,Text,new RectangleF(Danger?27:35,0,w-40,h),13,ink,false,false);
        if(Focused&&ShowFocusCues)using(var pen=new Pen(LayoutVisuals.Blue,1)){pen.DashStyle=DashStyle.Dot;e.Graphics.DrawRectangle(pen,3,3,w-7,h-7);}
    }
}

internal sealed class LayoutStatusStrip:Control
{
    public LayoutUiState Snapshot;public string Pending;
    public LayoutStatusStrip(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);TabStop=false;}
    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.Clear(BackColor);float s=LayoutVisuals.Begin(e.Graphics,this),w=Width/s,h=Height/s;
        LayoutVisuals.Box(e.Graphics,new RectangleF(1,1,w-2,h-2),Color.FromArgb(247,249,252),LayoutVisuals.Line,8);
        if(Snapshot==null)return;
        using(var pen=new Pen(LayoutVisuals.Line,1)){e.Graphics.DrawLine(pen,w/3,10,w/3,h-10);e.Graphics.DrawLine(pen,w*2/3,10,w*2/3,h-10);}
        Color dot=Snapshot.Failed?Color.FromArgb(231,113,54):(Snapshot.Connected?Color.FromArgb(33,191,105):Color.FromArgb(148,159,175));
        using(var b=new SolidBrush(dot))e.Graphics.FillEllipse(b,27,h/2-5,10,10);
        LayoutVisuals.Text(e.Graphics,String.IsNullOrEmpty(Pending)?Snapshot.StatusText:Pending,new RectangleF(47,0,w/3-55,h),12.5f,LayoutVisuals.Ink,false,false);
        LayoutVisuals.Symbol(e.Graphics,"grid",w/3+39,h/2-10,21,LayoutVisuals.Muted);
        LayoutVisuals.Text(e.Graphics,Snapshot.GridText,new RectangleF(w/3+70,0,w/3-78,h),12.5f,LayoutVisuals.Ink,false,false);
        LayoutVisuals.Symbol(e.Graphics,"check",w*2/3+37,h/2-10,21,LayoutVisuals.Muted);
        string saved=Snapshot.Failed?"需要查看运行信息":(Snapshot.Mode=="system"?"模式已记住":(Snapshot.GridReady?"布局已保存":"模式已记住"));
        LayoutVisuals.Text(e.Graphics,saved,new RectangleF(w*2/3+70,0,w/3-78,h),12.5f,LayoutVisuals.Ink,false,false);
    }
}

internal sealed class LayoutHeaderMark:Control
{
    public LayoutHeaderMark(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);TabStop=false;}
    protected override void OnPaint(PaintEventArgs e){LayoutVisuals.Begin(e.Graphics,this);LayoutVisuals.Mark(e.Graphics,0,0,47,LayoutVisuals.Blue);}
}

internal sealed class LayoutControlWindow:Form
{
    readonly Func<LayoutUiState> read;readonly Func<string,string> run;readonly Func<int> uninstall;
    readonly Panel canvas=new Panel();readonly LayoutModeCard[] cards=new LayoutModeCard[4];
    readonly LayoutToggle startup=new LayoutToggle();readonly LayoutStatusStrip status=new LayoutStatusStrip();
    readonly LayoutActionButton disclosure=new LayoutActionButton(),remove=new LayoutActionButton();
    readonly Label connection=new Label(),brand=new Label();readonly Panel details=new Panel();readonly TextBox output=new TextBox();
    readonly System.Windows.Forms.Timer timer=new System.Windows.Forms.Timer();
    bool syncing,busy,expanded;string lastMessage="模式和图标位置会自动保存。";
    public LayoutControlWindow(Func<LayoutUiState> readState,Func<string,string> runAction,Func<int> uninstallAction)
    {
        read=readState;run=runAction;uninstall=uninstallAction;RenderScale=1;
        AutoScaleMode=AutoScaleMode.None;
        Text="开始菜单自由布局";Font=new Font("Microsoft YaHei UI",9.5f);BackColor=Color.White;
        ClientSize=new Size(930,660);StartPosition=FormStartPosition.CenterScreen;FormBorderStyle=FormBorderStyle.FixedSingle;MaximizeBox=false;AutoScroll=true;
        Icon=LayoutVisuals.AppIcon();canvas.Size=new Size(930,660);canvas.BackColor=Color.White;Controls.Add(canvas);
        var mark=new LayoutHeaderMark {Bounds=new Rectangle(30,20,50,50)};canvas.Controls.Add(mark);
        AddLabel(canvas,"开始菜单自由布局",new Rectangle(98,20,460,32),17,true,LayoutVisuals.Ink);
        AddLabel(canvas,"管理布局模式与启动选项",new Rectangle(99,53,460,23),10,false,LayoutVisuals.Muted);
        connection.Bounds=new Rectangle(758,37,142,28);connection.TextAlign=ContentAlignment.MiddleRight;connection.ForeColor=Color.FromArgb(32,154,89);canvas.Controls.Add(connection);
        var modes=new LayoutPanel {Bounds=new Rectangle(18,88,894,350),AccessibleName="布局模式"};canvas.Controls.Add(modes);
        AddLabel(modes,"布局模式",new Rectangle(16,13,93,28),13.5f,true,LayoutVisuals.Ink);
        AddLabel(modes,"一次选择，自动记住",new Rectangle(108,15,285,25),10,false,LayoutVisuals.Muted);
        string[] modeNames={"free","locked","arrange","system"};
        string[] titles={"自由拖动＋点击","锁定位置使用","只整理图标","系统原生布局"};
        string[] descriptions={"直接拖动 · 单击打开","Ctrl＋拖动 · 单击打开","直接拖动 · 暂停打开","使用系统原生排列"};
        for(int i=0;i<4;i++) {
            int index=i;var card=new LayoutModeCard(modeNames[i],titles[i],descriptions[i]) {Bounds=new Rectangle(16+(i%2)*438,47+(i/2)*146,424,135),TabIndex=i};
            card.Click+=delegate {Apply(index==0?"free":index==1?"locked":index==2?"edit":"restore");};
            card.KeyDown+=(sender,e)=> {
                int target=index;
                if(e.KeyCode==Keys.Left&&index%2==1)target=index-1;
                if(e.KeyCode==Keys.Right&&index%2==0)target=index+1;
                if(e.KeyCode==Keys.Up&&index>=2)target=index-2;
                if(e.KeyCode==Keys.Down&&index<2)target=index+2;
                if(target!=index){cards[target].Focus();e.Handled=true;e.SuppressKeyPress=true;}
            };
            cards[i]=card;modes.Controls.Add(card);
        }
        var startupRow=new LayoutPanel {Bounds=new Rectangle(18,450,894,60)};canvas.Controls.Add(startupRow);
        startupRow.Paint+=(sender,e)=> {var saved=e.Graphics.Save();LayoutVisuals.Begin(e.Graphics,this);LayoutVisuals.Symbol(e.Graphics,"power",20,17,29,LayoutVisuals.Ink);e.Graphics.Restore(saved);};
        AddLabel(startupRow,"开机自启动",new Rectangle(69,9,490,24),11.5f,true,LayoutVisuals.Ink);
        AddLabel(startupRow,"登录后恢复上次模式和图标位置",new Rectangle(69,32,620,21),9.5f,false,LayoutVisuals.Muted);
        startup.Bounds=new Rectangle(785,14,96,34);startup.TabIndex=4;
        startup.CheckedChanged+=delegate {if(!syncing)Apply(startup.Checked?"startup-on":"startup-off");};startupRow.Controls.Add(startup);
        status.Bounds=new Rectangle(18,522,894,40);canvas.Controls.Add(status);
        disclosure.Bounds=new Rectangle(18,574,894,34);disclosure.Text="运行信息";disclosure.AccessibleName="展开运行信息";disclosure.Disclosure=true;disclosure.TabIndex=5;
        disclosure.Click+=delegate {SetExpanded(!expanded);};canvas.Controls.Add(disclosure);
        details.Bounds=new Rectangle(19,615,892,140);details.Visible=false;details.BackColor=Color.FromArgb(248,250,252);canvas.Controls.Add(details);
        output.Bounds=new Rectangle(14,9,864,122);output.Multiline=true;output.ReadOnly=true;output.BorderStyle=BorderStyle.None;output.BackColor=details.BackColor;output.ForeColor=LayoutVisuals.Muted;
        output.ScrollBars=ScrollBars.Vertical;output.AccessibleName="运行信息内容";output.TabStop=true;details.Controls.Add(output);
        remove.Bounds=new Rectangle(18,620,225,29);remove.Text="卸载并清除数据";remove.AccessibleName=remove.Text;remove.Danger=true;remove.TabIndex=7;
        remove.Click+=delegate {try {if(uninstall()==0)Close();}catch(Exception ex){ShowError(ex);}};canvas.Controls.Add(remove);
        brand.Bounds=new Rectangle(629,622,280,25);brand.Text="StartMenuFreeLayout";brand.TextAlign=ContentAlignment.MiddleRight;brand.ForeColor=LayoutVisuals.Muted;canvas.Controls.Add(brand);
        timer.Interval=800;timer.Tick+=delegate {if(!busy)RefreshState();};
        Shown+=delegate {FitWindow();RefreshState();timer.Start();};
        FormClosing+=(sender,e)=> {if(busy&&e.CloseReason==CloseReason.UserClosing){e.Cancel=true;status.Pending="正在应用，请稍候";status.Invalidate();}};
        RefreshState();
    }
    static void AddLabel(Control parent,string text,Rectangle bounds,float size,bool bold,Color color)
    {
        parent.Controls.Add(new Label {Text=text,Bounds=bounds,Font=new Font("Microsoft YaHei UI",size,bold?FontStyle.Bold:FontStyle.Regular),ForeColor=color,BackColor=Color.White,TextAlign=ContentAlignment.MiddleLeft,AutoEllipsis=true});
    }
    [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr window);
    public float RenderScale {get;private set;}
    protected override void OnLoad(EventArgs e)
    {
        uint dpi=GetDpiForWindow(Handle);RenderScale=dpi==0?1:dpi/96f;
        if(Math.Abs(RenderScale-1)>.001f)Scale(new SizeF(RenderScale,RenderScale));
        base.OnLoad(e);
    }
    float ScaleFactor {get {return RenderScale>0?RenderScale:1;}}
    int D(float logical){return (int)Math.Round(logical*ScaleFactor);}
    void FitWindow()
    {
        AutoScrollMinSize=canvas.Size;var area=Screen.FromControl(this).WorkingArea;
        int desiredClientHeight=D(expanded?813:660),chrome=Height-ClientSize.Height;
        ClientSize=new Size(Math.Min(D(930),area.Width-32),Math.Min(desiredClientHeight,area.Height-chrome-32));
        Location=new Point(area.Left+(area.Width-Width)/2,area.Top+(area.Height-Height)/2);
    }
    void SetExpanded(bool value)
    {
        expanded=value;disclosure.Expanded=value;disclosure.AccessibleName=value?"收起运行信息":"展开运行信息";disclosure.Invalidate();
        details.Visible=value;remove.Top=D(value?773:620);brand.Top=D(value?775:622);canvas.Height=D(value?813:660);
        FitWindow();
    }
    void RefreshState()
    {
        try {
            var snapshot=read();syncing=true;startup.Checked=snapshot.AutoStart;syncing=false;
            foreach(var card in cards)card.Chosen=card.Mode==snapshot.Mode;
            connection.Text=busy?"●  正在应用":(snapshot.Failed?"●  需要检查":(snapshot.Connected?"●  已连接":"●  等待连接"));
            connection.ForeColor=snapshot.Failed?Color.FromArgb(211,113,49):(snapshot.Connected?Color.FromArgb(28,169,91):LayoutVisuals.Muted);
            status.Snapshot=snapshot;status.AccessibleName=snapshot.StatusText+"；"+snapshot.GridText;status.Invalidate();
            string information="当前状态："+snapshot.StatusText+"\r\n已记住模式："+ModePolicy.Name(snapshot.Mode)+"\r\n\r\n"+lastMessage;
            if(snapshot.Failed&&!String.IsNullOrEmpty(snapshot.StartupError))information+="\r\n"+snapshot.StartupError;
            if(output.Text!=information)output.Text=information;
        }catch(Exception ex){syncing=false;lastMessage=ex.Message;output.Text="读取状态失败："+ex.Message;connection.Text="●  读取失败";connection.ForeColor=Color.FromArgb(211,113,49);}
    }
    void Apply(string action)
    {
        if(busy)return;busy=true;SetBusy(true);status.Pending="正在应用，请稍候";status.Invalidate();connection.Text="●  正在应用";
        var thread=new Thread(delegate() {
            string message=null;Exception error=null;try {message=run(action);}catch(Exception ex){error=ex;}
            if(IsDisposed||Disposing)return;
            try {BeginInvoke(new MethodInvoker(delegate {
                busy=false;SetBusy(false);status.Pending=null;
                if(error!=null)ShowError(error);else {lastMessage=message;RefreshState();}
            }));}catch(InvalidOperationException){}
        });thread.IsBackground=true;thread.SetApartmentState(ApartmentState.STA);thread.Start();
    }
    void SetBusy(bool value){foreach(var card in cards)card.Enabled=!value;startup.Enabled=!value;remove.Enabled=!value;}
    void ShowError(Exception error){lastMessage="操作未完成："+error.Message;RefreshState();SetExpanded(true);}
    protected override void Dispose(bool disposing)
    {
        if(disposing){timer.Stop();timer.Dispose();if(Icon!=null)Icon.Dispose();}base.Dispose(disposing);
    }
}
#endif
