// Experimental native Start menu extension. MIT license; see LICENSE.txt.
// Target verified by launcher: Windows 11 26200.7840 / Start host 26100.7309.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <xamlom.h>
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#if defined(__MINGW32__)
__CRT_UUID_DECL(IXamlDiagnostics,0x18C9E2B6,0x3F43,0x4116,0x9F,0x2B,0xFF,0x93,0x5D,0x77,0x70,0xD2)
__CRT_UUID_DECL(IVisualTreeService,0xA593B11A,0xD17F,0x48BB,0x8F,0x66,0x83,0x91,0x07,0x31,0xC8,0xA5)
__CRT_UUID_DECL(IVisualTreeServiceCallback2,0xBAD9EB88,0xAE77,0x4397,0xB9,0x48,0x5F,0xA2,0xDB,0x0A,0x19,0xEA)
#endif
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "GridLayout.h"
#include "PointerGesture.h"
#include "NativeFolders.h"

namespace wf = winrt::Windows::Foundation;
namespace wx = winrt::Windows::UI::Xaml;
namespace wc = winrt::Windows::UI::Xaml::Controls;
namespace wi = winrt::Windows::UI::Xaml::Input;
namespace wm = winrt::Windows::UI::Xaml::Media;
constexpr CLSID kClassId{0xa8e1f4d2,0x35e2,0x4cf6,{0x8c,0x2a,0x96,0x12,0x5b,0x47,0xee,0x71}};
HMODULE g_module{};
std::wstring g_dir;
std::mutex g_logMutex;

std::wstring IniPath() { return g_dir + L"\\layout.ini"; }
void Log(const std::wstring& message) noexcept {
    try {
        std::lock_guard lock(g_logMutex);
        SYSTEMTIME t{}; GetLocalTime(&t);
        wchar_t prefix[100];
        swprintf(prefix,100,L"%02d:%02d:%02d [%lu] ",t.wHour,t.wMinute,t.wSecond,GetCurrentThreadId());
        auto s = std::wstring(prefix) + message + L"\r\n";
        int n = WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);
        std::string bytes(n,'\0');
        WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),bytes.data(),n,nullptr,nullptr);
        HANDLE f = CreateFileW((g_dir+L"\\extension.log").c_str(),FILE_APPEND_DATA,
            FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (f != INVALID_HANDLE_VALUE) { DWORD written{}; WriteFile(f,bytes.data(),DWORD(bytes.size()),&written,nullptr); CloseHandle(f); }
    } catch (...) {}
}
void Error(const wchar_t* context) noexcept {
    try { auto e=winrt::hresult_error(winrt::to_hresult()); Log(std::wstring(context)+L": "+e.message().c_str()); } catch (...) {}
}
std::wstring Read(const wchar_t* section,const wchar_t* key,const wchar_t* fallback=L"") {
    wchar_t b[1024]; GetPrivateProfileStringW(section,key,fallback,b,1024,IniPath().c_str()); return b;
}
int Number(const wchar_t* key,int fallback) { return int(GetPrivateProfileIntW(L"Settings",key,fallback,IniPath().c_str())); }
void Status(const wchar_t* text) { if(Read(L"Runtime",L"Status")!=text)WritePrivateProfileStringW(L"Runtime",L"Status",text,IniPath().c_str()); }
std::wstring Hash(const std::wstring& s) {
    unsigned long long n=14695981039346656037ULL;
    for (auto c:s) { n^=static_cast<unsigned short>(c); n*=1099511628211ULL; }
    wchar_t b[24]; swprintf(b,24,L"%016llx",n); return b;
}
wx::DependencyObject Find(wx::DependencyObject const& root, const std::function<bool(wx::DependencyObject const&)>& test,int depth=0) {
    if (!root || depth>25) return nullptr;
    if(test(root)) return root;
    int n=wm::VisualTreeHelper::GetChildrenCount(root);
    for(int i=0;i<n;++i) if(auto r=Find(wm::VisualTreeHelper::GetChild(root,i),test,depth+1)) return r;
    return nullptr;
}
std::wstring Label(wc::GridViewItem const& item) {
    auto name=winrt::Windows::UI::Xaml::Automation::AutomationProperties::GetName(item);
    if(!name.empty()) return name.c_str();
    auto text=Find(item,[](auto const& d) { auto t=d.template try_as<wc::TextBlock>(); return t && !t.Text().empty(); });
    return text ? std::wstring(text.as<wc::TextBlock>().Text().c_str()) : L"";
}
struct TransitionState {
    winrt::weak_ref<wx::DependencyObject> element;
    wx::DependencyProperty property{nullptr};
    wf::IInspectable original{nullptr};
    wm::Animation::TransitionCollection empty{nullptr};
};
void SuppressLayoutTransitions(std::vector<TransitionState>& states,wx::DependencyObject const& element,wx::DependencyProperty const& property) {
    if(!element)return;
    auto found=std::find_if(states.begin(),states.end(),[&](auto const& s){return s.element.get()==element&&s.property==property;});
    if(found==states.end()) {
        auto transitions=element.GetValue(property).try_as<wm::Animation::TransitionCollection>();
        if(transitions&&transitions.Size()>0) {
            std::wstring names;for(auto const& t:transitions)names+=std::wstring(winrt::get_class_name(t))+L";";
            Log(L"Suppress container layout transitions: "+names);
        }
        states.push_back({winrt::make_weak(element),property,element.ReadLocalValue(property),wm::Animation::TransitionCollection{}});
        found=std::prev(states.end());
    }
    if(element.GetValue(property)!=found->empty||found->empty.Size()>0) {
        if(found->empty.Size()>0)found->empty.Clear();
        element.SetValue(property,found->empty);
    }
}
void RestoreLayoutTransitions(std::vector<TransitionState>& states) noexcept {
    for(auto it=states.rbegin();it!=states.rend();++it)try {
        if(auto element=it->element.get()) {
            if(it->original==wx::DependencyProperty::UnsetValue())element.ClearValue(it->property);
            else element.SetValue(it->property,it->original);
        }
    }catch(...){Error(L"Restore layout transitions");}
    states.clear();
}
struct Item {
    winrt::weak_ref<wc::GridViewItem> view;
    std::wstring key,label,legacyKey;
    wf::IInspectable model{nullptr};
    int index{};
    double x{},y{};
    layout::Cell cell{},requested{};
    bool saved{},assigned{},capturing{};
    input::Gesture gesture;
    wf::Numerics::float3 originalTranslation{};
    double offsetX{},offsetY{};
    wf::Point anchor{},down{},start{};
    unsigned pointer{};
    wi::PointerEventHandler pressed{nullptr},moved{nullptr},released{nullptr},lost{nullptr};
    wf::IInspectable handlers[4]{nullptr,nullptr,nullptr,nullptr};
    bool attached[4]{};
    std::vector<TransitionState> transitions;
};
struct SpaceState {
    winrt::weak_ref<wx::FrameworkElement> element;
    double height{},minimum{},maximum{},extra{};
    wm::RectangleGeometry clip{nullptr};
};
struct Controller:std::enable_shared_from_this<Controller> {
    winrt::weak_ref<wc::GridView> weakGrid;
    winrt::weak_ref<wc::ScrollViewer> viewport;
    winrt::weak_ref<wx::FrameworkElement> pinnedHost;
    wx::FrameworkElement surface{nullptr};
    bool active{},logged{},arranged{},oldDrag{},oldReorder{},oldClick{},oldSwipe{},clickHooked{};
    int inputMode{}; // 0: Ctrl drag, 1: arrange only, 2: click and direct drag
    winrt::event_token clickedToken{};
    layout::Grid grid;
    double boardWidth{},boardHeight{};
    std::map<int,wf::Point> initial;
    std::map<uintptr_t,std::shared_ptr<Item>> items;
    std::vector<SpaceState> spaces;
    std::vector<TransitionState> transitions;
    std::wstring snapshotToken;
    bool folderPending{},folderAppend{},folderCollapse{},inFastLayout{},layoutHooked{};
    winrt::event_token layoutToken{},renderToken{};
    bool renderHooked{};
    std::map<std::wstring,layout::Cell> persisted;
    std::map<std::wstring,layout::Cell> placements;
    std::weak_ptr<Item> hintItem;
    ULONGLONG lastSingletonCheck{};
    ULONGLONG duplicateRepairAfter{GetTickCount64()+2000};
    std::wstring replacementKey;
    ULONGLONG folderDeadline{};
    layout::Cell folderCell{};
    std::wstring folderSource,folderTarget,folderMemberId;
    std::vector<std::wstring> beforeFolder;
    explicit Controller(wc::GridView const& g):weakGrid(g){}
    void Runtime(const wchar_t* key,std::wstring const& value) {
        if(Read(L"Runtime",key)!=value)WritePrivateProfileStringW(L"Runtime",key,value.c_str(),IniPath().c_str());
    }
    std::vector<std::shared_ptr<Item>> Ordered() {
        std::vector<std::shared_ptr<Item>> result;for(auto const& [_,p]:items)if(p->view.get())result.push_back(p);
        std::sort(result.begin(),result.end(),[](auto const& a,auto const& b){return a->index<b->index;});return result;
    }
    void DumpGeometry(wc::GridView const& g) {
        Log(L"--- Native layout geometry ---");
        if(nativefolders::Compatible())for(unsigned i=0;i<g.Items().Size();++i)if(auto view=g.ContainerFromIndex(int(i)).try_as<wc::GridViewItem>()) {
            auto model=Model(view,int(i));if(!model)continue;
            Log(L"Pin audit: "+Label(view)+L" id="+nativefolders::Id(model));
            if(nativefolders::Query(model,nativefolders::Folder))for(auto const& member:nativefolders::Children(model))Log(L"  Folder member id="+nativefolders::Id(member));
        }
        wx::DependencyObject node=g;
        for(int depth=0;node&&depth<24;depth++) {
            if(auto fe=node.try_as<wx::FrameworkElement>()) {
                auto point=g.TransformToVisual(fe).TransformPoint({0,0});
                auto line=std::wstring(winrt::get_class_name(fe).c_str())+L"#"+fe.Name().c_str()+L" size="+std::to_wstring(fe.ActualWidth())+L","+std::to_wstring(fe.ActualHeight())+L" gridY="+std::to_wstring(point.Y);
                if(auto c=fe.Clip())line+=L" clipH="+std::to_wstring(c.Rect().Height);Log(line);
            }
            node=wm::VisualTreeHelper::GetParent(node);
        }
        if(auto panel=g.ItemsPanelRoot())Log(L"ItemsPanel height="+std::to_wstring(panel.ActualHeight()));
    }
    void Capture(wx::FrameworkElement const& element,double extra=0) {
        for(auto const& state:spaces)if(state.element.get()==element)return;
        spaces.push_back({winrt::make_weak(element),element.Height(),element.MinHeight(),element.MaxHeight(),extra,element.Clip()});
    }
    void CaptureSpaces(wc::GridView const& g) {
        wx::DependencyObject node=g;
        for(int depth=0;node&&depth<12;depth++) {
            auto fe=node.try_as<wx::FrameworkElement>();
            if(fe) {
                if(fe.Name()==L"TopLevelHeader") {
                    // Keep the title above the grid, but do not add the native
                    // trailing padding below it: that would create scrolling
                    // when the last cell ends exactly at the account bar.
                    auto origin=g.TransformToVisual(fe).TransformPoint({0,0});
                    Capture(fe,std::max(0.0,double(origin.Y)));break;
                }
                Capture(fe);
                if(fe.Name()==L"StartMenuPinnedList")pinnedHost=winrt::make_weak(fe);
            }
            node=wm::VisualTreeHelper::GetParent(node);
        }
        node=g.ItemsPanelRoot();
        for(int depth=0;node&&node!=g&&depth<20;depth++) {
            if(auto fe=node.try_as<wx::FrameworkElement>())Capture(fe);
            node=wm::VisualTreeHelper::GetParent(node);
        }
        node=wm::VisualTreeHelper::GetParent(g);
        for(int depth=0;node&&depth<30;depth++) {
            if(auto v=node.try_as<wc::ScrollViewer>()){viewport=winrt::make_weak(v);break;}
            node=wm::VisualTreeHelper::GetParent(node);
        }
    }
    void ApplySpace(wc::GridView const& g) {
        double available=std::max(336.0,g.ActualHeight());
        if(auto v=viewport.get()) {
            auto point=g.TransformToVisual(v).TransformPoint({0,0});
            available=v.ActualHeight()-std::max(0.0,double(point.Y)+v.VerticalOffset());
        }
        int columns=std::max(1,layout::FitCells(g.ActualWidth(),grid.width));
        int rows=std::max(1,layout::FitCells(available,grid.height));
        int requested=Number(L"GridRows",0);if(requested>0)rows=std::min(rows,requested);
        if(columns*rows<int(g.Items().Size()))throw winrt::hresult_error(E_BOUNDS,L"可见区域不足以容纳全部图标，已停止网格布局。");
        if(columns!=grid.columns||rows!=grid.rows){grid.columns=columns;grid.rows=rows;arranged=false;}
        boardWidth=grid.columns*grid.width;boardHeight=grid.rows*grid.height;
        for(auto const& state:spaces)if(auto fe=state.element.get()) {
            double h=boardHeight+state.extra;
            if(!std::isinf(fe.MaxHeight()))fe.MaxHeight(INFINITY);
            if(fe.MinHeight()!=h)fe.MinHeight(h);
            if(fe.Height()!=h)fe.Height(h);
            if(auto clip=fe.Clip();clip&&clip.Rect().Height<h) {
                auto rect=clip.Rect();rect.Height=float(h);wm::RectangleGeometry replacement;replacement.Rect(rect);fe.Clip(replacement);
            }
        }
        Runtime(L"Grid",std::to_wstring(grid.columns)+L"x"+std::to_wstring(grid.rows));
        Runtime(L"GridHeight",std::to_wstring(boardHeight));
        Runtime(L"AvailableHeight",std::to_wstring(available));
    }
    void Save(Item const& p) {
        auto previous=persisted.find(p.key);if(previous!=persisted.end()&&previous->second==p.cell)return;
        wchar_t cell[80],position[100];swprintf(cell,80,L"%d,%d",p.cell.column,p.cell.row);swprintf(position,100,L"%.2f,%.2f",grid.X(p.cell),grid.Y(p.cell));
        if(!WritePrivateProfileStringW(L"Cells",p.key.c_str(),cell,IniPath().c_str())||!WritePrivateProfileStringW(L"Positions",p.key.c_str(),position,IniPath().c_str())) {
            Log(L"Could not save cell, error="+std::to_wstring(GetLastError()));Status(L"save-failed");return;
        }else if(p.key!=p.legacyKey) {
            if(!Read(L"Cells",p.legacyKey.c_str()).empty())WritePrivateProfileStringW(L"Cells",p.legacyKey.c_str(),nullptr,IniPath().c_str());
            if(!Read(L"Positions",p.legacyKey.c_str()).empty())WritePrivateProfileStringW(L"Positions",p.legacyKey.c_str(),nullptr,IniPath().c_str());
        }
        persisted[p.key]=p.cell;placements[p.key]=p.cell;
    }
    void Position(std::shared_ptr<Item> const& p,double x,double y) {
        auto view=p->view.get();if(!view||!surface)return;
        p->x=std::clamp(x,0.0,std::max(0.0,boardWidth-view.ActualWidth()));
        p->y=std::clamp(y,0.0,std::max(0.0,boardHeight-view.ActualHeight()));
        p->offsetX=p->x-p->anchor.X;p->offsetY=p->y-p->anchor.Y;
        auto desired=p->originalTranslation;
        desired.x+=float(p->offsetX);desired.y+=float(p->offsetY);
        auto actual=view.Translation();
        if(std::abs(actual.x-desired.x)>.01||std::abs(actual.y-desired.y)>.01||std::abs(actual.z-desired.z)>.01)view.Translation(desired);
    }
    void UpdateAnchor(std::shared_ptr<Item> const& p,wc::GridView const& g) {
        auto view=p->view.get();auto panel=g.ItemsPanelRoot();if(!view||!panel)return;
        auto slot=wc::Primitives::LayoutInformation::GetLayoutSlot(view);
        auto origin=panel.TransformToVisual(g).TransformPoint({0,0});
        if(slot.Width>0&&slot.Height>0)p->anchor={origin.X+slot.X,origin.Y+slot.Y};
    }
    void ClearHint() {
        if(auto p=hintItem.lock())if(auto view=p->view.get()) {
            try {
                wx::VisualStateManager::GoToState(view,L"NoFolderDropHint",true);
                auto control=Find(view,[](auto const& d){auto name=winrt::get_class_name(d);return name==L"StartMenu.PinnedListTile"||name==L"StartMenu.PinnedListCollapsedFolder";});
                if(auto c=control.try_as<wc::Control>())wx::VisualStateManager::GoToState(c,L"NoFolderDropHint",true);
            }catch(...){Error(L"Clear folder hint");}
        }
        hintItem.reset();
    }
    void Hint(std::shared_ptr<Item> const& source) {
        auto destination=grid.Snap(source->x,source->y);std::shared_ptr<Item> target;
        if(nativefolders::Query(source->model,nativefolders::Tile))for(auto const& p:Ordered())if(p!=source&&p->cell==destination){target=p;break;}
        if(hintItem.lock()==target)return;ClearHint();if(!target)return;
        auto view=target->view.get();if(!view)return;
        bool animate=winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled();
        bool shown=wx::VisualStateManager::GoToState(view,L"FolderDropHint",animate);
        auto control=Find(view,[](auto const& d){auto name=winrt::get_class_name(d);return name==L"StartMenu.PinnedListTile"||name==L"StartMenu.PinnedListCollapsedFolder";});
        if(auto c=control.try_as<wc::Control>())shown=wx::VisualStateManager::GoToState(c,L"FolderDropHint",animate)||shown;
        hintItem=target;Log(shown?L"Native FolderDropHint animation started":L"Native FolderDropHint state unavailable");
    }
    static bool ControlDown() {return (GetAsyncKeyState(VK_CONTROL)&0x8000)!=0;}
    void CancelDrag(std::shared_ptr<Item> const& p) {
        if(!p->gesture.Active())return;
        ClearHint();
        bool dragging=p->gesture.Dragging();p->gesture.Cancel();
        if(dragging)if(auto view=p->view.get())view.ReleasePointerCaptures();
        Position(p,grid.X(p->cell),grid.Y(p->cell));
    }
    void SyncInput(wc::GridView const& g) {
        int requested=std::clamp(Number(L"EditMode",0),0,2);
        if(requested!=inputMode) {
            for(auto const& [_,p]:items)CancelDrag(p);
            inputMode=requested;
            Log(L"Input mode: "+std::to_wstring(inputMode));
        }
        // The native menu can rewrite these properties after its own refresh.
        // Its reordering must stay off in both modes, otherwise two independent
        // drag implementations can capture the same pointer.
        if(g.CanDragItems())g.CanDragItems(false);
        if(g.CanReorderItems())g.CanReorderItems(false);
        if(g.IsSwipeEnabled())g.IsSwipeEnabled(false);
        bool dragging=false;
        for(auto const& [_,p]:items)dragging|=p->gesture.Dragging();
        bool click=inputMode!=1&&!dragging&&oldClick;
        if(g.IsItemClickEnabled()!=click)g.IsItemClickEnabled(click);
        Runtime(L"InputMode",inputMode==2?L"free":(inputMode==1?L"edit":L"use"));
        Runtime(L"NativeClick",g.IsItemClickEnabled()?L"enabled":L"disabled");
        Runtime(L"NativeDrag",g.CanDragItems()?L"enabled":L"disabled");
    }
    void Arrange() {
        auto ordered=Ordered();std::vector<layout::Request> requests;
        for(auto const& p:ordered)requests.push_back({p->assigned?p->cell:p->requested,p->saved||p->assigned});
        auto resolved=layout::Resolve(grid,requests);
        for(size_t i=0;i<ordered.size();i++) {
            auto p=ordered[i];p->cell=resolved[i];p->assigned=true;placements[p->key]=p->cell;Position(p,grid.X(p->cell),grid.Y(p->cell));Save(*p);
        }
        WritePrivateProfileStringW(L"Settings",L"LayoutVersion",L"2",IniPath().c_str());arranged=true;
        Log(L"Grid arranged: "+std::to_wstring(grid.columns)+L" columns, "+std::to_wstring(grid.rows)+L" rows; items="+std::to_wstring(ordered.size()));
    }
    wf::IInspectable Model(wc::GridViewItem const& view,int index) {
        auto g=weakGrid.get();if(!g)return nullptr;
        std::vector<wf::IInspectable> candidates{g.Items().GetAt(index),view.Content(),view.DataContext()};
        for(auto const& candidate:candidates)if(nativefolders::Query(candidate,nativefolders::Item))return candidate;
        return nullptr;
    }
    wf::IInspectable FolderViewModel() {
        if(!nativefolders::Compatible())return nullptr;
        if(auto host=pinnedHost.get())if(auto vm=nativefolders::ViewModel(host))return vm;
        if(auto g=weakGrid.get())return nativefolders::ViewModel(g.DataContext());
        return nullptr;
    }
    void Group(std::shared_ptr<Item> const& source,std::shared_ptr<Item> const& target) {
        if(folderPending||!nativefolders::Compatible()||!nativefolders::Query(source->model,nativefolders::Tile)) {
            Position(source,grid.X(source->cell),grid.Y(source->cell));ClearHint();Runtime(L"FolderAction",L"unsupported");return;
        }
        auto vm=FolderViewModel();if(!vm){Runtime(L"FolderAction",L"unavailable");return;}
        folderCollapse=false;folderAppend=bool(nativefolders::Query(target->model,nativefolders::Folder));
        if(!folderAppend&&!nativefolders::Query(target->model,nativefolders::Tile)){Runtime(L"FolderAction",L"unsupported");return;}
        beforeFolder.clear();for(auto const& p:Ordered())beforeFolder.push_back(p->key);
        folderCell=target->cell;folderSource=source->key;folderTarget=target->key;
        folderMemberId=nativefolders::Id(source->model);
        folderPending=true;folderDeadline=GetTickCount64()+5000;
        try {
            if(folderAppend)nativefolders::MoveInto(vm,target->model,source->model);
            else nativefolders::Create(vm,source->model,target->model);
            arranged=false;Runtime(L"FolderAction",L"pending");
            Log(L"Native folder request: "+source->label+L" -> "+target->label);
            ClearHint();FastLayout();
        }catch(...) {folderPending=false;Position(source,grid.X(source->cell),grid.Y(source->cell));ClearHint();Error(L"Native folder request");Runtime(L"FolderAction",L"failed");}
    }
    void FinishFolder() {
        if(!folderPending)return;
        bool sourcePresent=false,targetPresent=false;std::shared_ptr<Item> result;
        for(auto const& p:Ordered()) {
            sourcePresent|=p->key==folderSource;targetPresent|=p->key==folderTarget;
            if(folderCollapse&&p->key==replacementKey)result=p;
            if(!folderCollapse&&folderAppend&&p->key==folderTarget)result=p;
            if(!folderCollapse&&!folderAppend&&std::find(beforeFolder.begin(),beforeFolder.end(),p->key)==beforeFolder.end()&&nativefolders::Query(p->model,nativefolders::Folder))result=p;
        }
        bool memberVerified=!folderAppend||folderCollapse||(result&&nativefolders::Count(result->model,folderMemberId)==1);
        if(!sourcePresent&&result&&memberVerified&&(folderAppend||!targetPresent)) {
            result->cell=folderCell;result->requested=folderCell;result->saved=true;result->assigned=true;Save(*result);
            placements[result->key]=folderCell;
            folderPending=false;arranged=false;Runtime(L"FolderAction",folderCollapse?L"collapsed":(folderAppend?L"added":L"created"));
            Log(L"Native folder completed at cell="+std::to_wstring(folderCell.column)+L","+std::to_wstring(folderCell.row)+(folderAppend?L"; member verified once, root absent":L""));
        }else if(GetTickCount64()>folderDeadline) {
            folderPending=false;arranged=false;Runtime(L"FolderAction",L"not-created");Log(L"Native folder operation did not change the pinned list; original cells preserved");
        }
    }
    void Drop(std::shared_ptr<Item> const& p) {
        auto ordered=Ordered();std::vector<layout::Cell> cells;size_t index=ordered.size();
        for(size_t i=0;i<ordered.size();i++){cells.push_back(ordered[i]->cell);if(ordered[i]==p)index=i;}
        auto destination=grid.Snap(p->x,p->y);
        for(auto const& target:ordered)if(target!=p&&target->cell==destination){Group(p,target);return;}
        ClearHint();
        if(!layout::Move(grid,cells,index,destination))return;
        for(size_t i=0;i<ordered.size();i++)if(ordered[i]==p||ordered[i]->cell!=cells[i]) {
            auto item=ordered[i];item->cell=cells[i];item->saved=true;Position(item,grid.X(item->cell),grid.Y(item->cell));Save(*item);
        }
        Log(L"Dropped "+p->label+L" cell="+std::to_wstring(p->cell.column)+L","+std::to_wstring(p->cell.row));
    }
    void Detach(std::shared_ptr<Item> const& p) noexcept {
        auto view=p->view.get();
        if(view) {
            auto events=std::vector<wx::RoutedEvent>{wx::UIElement::PointerPressedEvent(),wx::UIElement::PointerMovedEvent(),wx::UIElement::PointerReleasedEvent(),wx::UIElement::PointerCaptureLostEvent()};
            for(int i=0;i<4;i++)if(p->attached[i])try{view.RemoveHandler(events[i],p->handlers[i]);}catch(...){Error(L"RemoveHandler");}
            try{p->gesture.Cancel();view.ReleasePointerCaptures();view.Translation(p->originalTranslation);}catch(...){Error(L"RestoreItem");}
        }
        for(auto& handler:p->handlers)handler=nullptr;
        RestoreLayoutTransitions(p->transitions);
        p->pressed=nullptr;p->moved=nullptr;p->released=nullptr;p->lost=nullptr;
    }
    void Bind(wc::GridViewItem const& view,int index) {
        auto id=reinterpret_cast<uintptr_t>(winrt::get_abi(view));auto label=Label(view);if(label.empty())return;
        auto model=nativefolders::Compatible()?Model(view,index):nullptr;
        auto nativeId=model?nativefolders::Id(model):L"";
        auto existing=items.find(id);if(existing!=items.end()) {
            if(existing->second->label==label&&existing->second->view.get()==view&&(nativeId.empty()||existing->second->key==Hash(L"native-id:"+nativeId))) {
                existing->second->index=index;existing->second->model=model;return;
            }
            Detach(existing->second);items.erase(existing);
        }
        auto p=std::make_shared<Item>();p->view=winrt::make_weak(view);p->label=label;p->index=index;p->legacyKey=Hash(label);
        int duplicate=0;for(auto const& [_,other]:items)if(other->label==label)duplicate++;
        if(duplicate)p->legacyKey+=L"-"+std::to_wstring(duplicate);
        p->model=model;
        p->key=nativeId.empty()?p->legacyKey:Hash(L"native-id:"+nativeId);
        auto g=weakGrid.get();if(!g)return;
        SuppressLayoutTransitions(p->transitions,view,wx::UIElement::TransitionsProperty());
        SuppressLayoutTransitions(p->transitions,view,wc::ContentControl::ContentTransitionsProperty());
        p->originalTranslation=view.Translation();items[id]=p;UpdateAnchor(p,g);
        auto fallback=initial.contains(index)?initial[index]:wf::Point{float((index%grid.columns)*grid.width),float((index/grid.columns)*grid.height)};
        p->requested=grid.Snap(fallback.X,fallback.Y);
        auto stored=Read(L"Cells",p->key.c_str());bool nativeStored=!stored.empty();int column,row;
        if(stored.empty()&&p->key!=p->legacyKey)stored=Read(L"Cells",p->legacyKey.c_str());
        if(!stored.empty()&&swscanf(stored.c_str(),L"%d,%d",&column,&row)==2){p->requested=grid.Clamp({column,row});p->saved=true;if(nativeStored)persisted[p->key]=p->requested;}
        else {
            stored=Read(L"Positions",p->key.c_str());double x,y;
            if(!stored.empty()&&swscanf(stored.c_str(),L"%lf,%lf",&x,&y)==2&&std::isfinite(x)&&std::isfinite(y)){p->requested=grid.Snap(x,y);p->saved=true;}
        }
        if(auto cached=placements.find(p->key);cached!=placements.end()){p->requested=cached->second;p->saved=true;}
        if(folderPending&&((folderCollapse&&p->key==replacementKey)||(!folderCollapse&&!folderAppend&&std::find(beforeFolder.begin(),beforeFolder.end(),p->key)==beforeFolder.end()&&nativefolders::Query(p->model,nativefolders::Folder)))){p->requested=folderCell;p->saved=true;}
        p->cell=p->requested;p->assigned=p->saved;placements[p->key]=p->cell;
        p->x=grid.X(p->requested);p->y=grid.Y(p->requested);
        Position(p,p->x,p->y);
        std::weak_ptr<Controller> self=shared_from_this();
        p->pressed=[self,p](auto const& sender,wi::PointerRoutedEventArgs const& e) {
            try {
                auto c=self.lock();if(!c||!c->active||!c->surface||!p->assigned)return;
                if(!e.GetCurrentPoint(c->surface).Properties().IsLeftButtonPressed())return;
                auto g=c->weakGrid.get();if(!g)return;c->SyncInput(g);
                bool control=ControlDown();
                Log(L"Pointer press: "+p->label+L" mode="+std::to_wstring(c->inputMode)+L" ctrl="+(control?L"1":L"0"));
                if(c->inputMode==0&&!control)return;
                auto view=sender.template as<wc::GridViewItem>();p->down=e.GetCurrentPoint(c->surface).Position();p->start={float(p->x),float(p->y)};p->pointer=e.Pointer().PointerId();
                p->gesture.Begin(c->inputMode==2&&!control);
                // A possible click stays native until movement crosses the drag
                // threshold. Do not handle or steal pointer capture on press.
                if(!p->gesture.Dragging())return;
                g.IsItemClickEnabled(false);
                p->capturing=true;bool captured=view.CapturePointer(e.Pointer());p->capturing=false;
                if(!captured){p->gesture.Cancel();c->SyncInput(g);return;}e.Handled(true);
            }catch(...){Error(L"PointerPressed");}
        };
        p->moved=[self,p](auto const&,wi::PointerRoutedEventArgs const& e) {
            try {auto c=self.lock();if(!c||!c->active||!p->gesture.Active()||e.Pointer().PointerId()!=p->pointer)return;
                if(c->inputMode==0&&!ControlDown()){c->CancelDrag(p);e.Handled(true);return;}
                auto current=e.GetCurrentPoint(c->surface);if(!current.Properties().IsLeftButtonPressed())return;
                auto pt=current.Position();
                if(p->gesture.Move(pt.X-p->down.X,pt.Y-p->down.Y,std::clamp(Number(L"DragThreshold",6),3,20))) {
                    auto view=p->view.get();auto g=c->weakGrid.get();if(!view||!g){p->gesture.Cancel();return;}
                    g.IsItemClickEnabled(false);p->capturing=true;bool captured=view.CapturePointer(e.Pointer());p->capturing=false;
                    if(!captured){p->gesture.Cancel();c->SyncInput(g);return;}
                    Log(L"Direct drag started: "+p->label);
                }
                if(!p->gesture.Dragging())return;
                c->Position(p,p->start.X+pt.X-p->down.X,p->start.Y+pt.Y-p->down.Y);c->Hint(p);e.Handled(true);
            }catch(...){Error(L"PointerMoved");}
        };
        p->released=[self,p](auto const& sender,wi::PointerRoutedEventArgs const& e) {
            try{
                auto c=self.lock();if(!c||!p->gesture.Active()||e.Pointer().PointerId()!=p->pointer)return;
                if(p->gesture.End()!=input::Release::Drop)return; // unhandled native click
                sender.template as<wc::GridViewItem>().ReleasePointerCapture(e.Pointer());e.Handled(true);c->Drop(p);
            }catch(...){Error(L"PointerReleased");}
        };
        p->lost=[self,p](auto const& sender,wi::PointerRoutedEventArgs const& e) {
            try{
                // Child buttons lose capture when we begin a drag. That routed
                // event is not the end of the container's own drag.
                if(p->capturing||e.OriginalSource()!=sender)return;
                if(auto c=self.lock();c&&c->active&&p->gesture.Active()) {
                    // Lost capture is cancellation, not a deliberate drop onto
                    // another pin. Never create a persistent folder here.
                    c->CancelDrag(p);
                }
            }catch(...){Error(L"PointerCaptureLost");}
        };
        p->handlers[0]=winrt::box_value(p->pressed);p->handlers[1]=winrt::box_value(p->moved);p->handlers[2]=winrt::box_value(p->released);p->handlers[3]=winrt::box_value(p->lost);
        auto events=std::vector<wx::RoutedEvent>{wx::UIElement::PointerPressedEvent(),wx::UIElement::PointerMovedEvent(),wx::UIElement::PointerReleasedEvent(),wx::UIElement::PointerCaptureLostEvent()};
        for(int j=0;j<4;j++){view.AddHandler(events[j],p->handlers[j],true);p->attached[j]=true;}
        arranged=false;
    }
    void Restore() noexcept {
        if(!active)return;active=false;
        ClearHint();
        try{if(auto g=weakGrid.get())if(layoutHooked){g.LayoutUpdated(layoutToken);layoutHooked=false;}if(renderHooked){wm::CompositionTarget::Rendering(renderToken);renderHooked=false;}}catch(...){}
        for(auto const& [_,p]:items)Detach(p);items.clear();
        RestoreLayoutTransitions(transitions);
        try {
            for(auto it=spaces.rbegin();it!=spaces.rend();++it)if(auto fe=it->element.get()) {
                fe.MinHeight(0);fe.MaxHeight(INFINITY);fe.Height(it->height);fe.MinHeight(it->minimum);fe.MaxHeight(it->maximum);fe.Clip(it->clip);
            }
            spaces.clear();surface=nullptr;arranged=false;folderPending=false;placements.clear();persisted.clear();
            if(auto g=weakGrid.get()){
                if(clickHooked){g.ItemClick(clickedToken);clickHooked=false;}
                g.CanDragItems(oldDrag);g.CanReorderItems(oldReorder);g.IsSwipeEnabled(oldSwipe);g.IsItemClickEnabled(oldClick);
            }
            Status(L"restored");Log(L"Original pinned layout restored");
        }catch(...){Error(L"Restore");Status(L"restore-failed");}
    }
    void Enable(wc::GridView const& g) {
        initial.clear();for(unsigned i=0;i<g.Items().Size();i++)if(auto view=g.ContainerFromIndex(int(i)).try_as<wc::GridViewItem>())initial[int(i)]=view.TransformToVisual(g).TransformPoint({0,0});
        if(auto first=g.ContainerFromIndex(0).try_as<wc::GridViewItem>()){if(first.ActualWidth()>10)grid.width=first.ActualWidth();if(first.ActualHeight()>10)grid.height=first.ActualHeight();}
        oldDrag=g.CanDragItems();oldReorder=g.CanReorderItems();oldClick=g.IsItemClickEnabled();oldSwipe=g.IsSwipeEnabled();
        SuppressLayoutTransitions(transitions,g,wc::ItemsControl::ItemContainerTransitionsProperty());
        CaptureSpaces(g);active=true;surface=g;inputMode=std::clamp(Number(L"EditMode",0),0,2);SyncInput(g);ApplySpace(g);
        std::weak_ptr<Controller> self=shared_from_this();
        clickedToken=g.ItemClick([self](auto const&,auto const&){if(auto c=self.lock();c&&c->active)Log(L"Native ItemClick dispatched");});clickHooked=true;
        layoutToken=g.LayoutUpdated([self](auto const&,auto const&){if(auto c=self.lock())c->FastLayout();});layoutHooked=true;
        renderToken=wm::CompositionTarget::Rendering([self](auto const&,auto const&){if(auto c=self.lock())c->FastLayout();});renderHooked=true;
        Log(L"Native grid layout enabled; cell="+std::to_wstring(grid.width)+L","+std::to_wstring(grid.height));
        Runtime(L"FolderApi",FolderViewModel()?L"ready":L"unavailable");
        Runtime(L"LayoutTransitions",L"suppressed");
    }
    void Check(wc::GridView const& g) {
        bool within=true,aligned=true;std::vector<layout::Cell> occupied;
        for(auto const& [_,p]:items)if(auto view=p->view.get()) {
            auto point=view.TransformToVisual(g).TransformPoint({0,0});
            within&=point.X>=-1&&point.Y>=-1&&point.X+view.ActualWidth()<=boardWidth+1&&point.Y+view.ActualHeight()<=boardHeight+1;
            aligned&=p->gesture.Dragging()||hintItem.lock()==p||(std::abs(point.X-grid.X(p->cell))<1&&std::abs(point.Y-grid.Y(p->cell))<1);
            aligned&=std::find(occupied.begin(),occupied.end(),p->cell)==occupied.end();occupied.push_back(p->cell);
        }
        for(auto const& state:spaces)if(auto fe=state.element.get())within&=fe.ActualHeight()+1>=boardHeight+state.extra;
        if(auto v=viewport.get()) {
            auto origin=g.TransformToVisual(v).TransformPoint({0,0});
            within&=origin.Y>=-1&&origin.Y+boardHeight<=v.ActualHeight()+1;
            within&=v.VerticalOffset()<1;
        }
        Runtime(L"BoundsCheck",within?L"pass":L"pending");Runtime(L"GridCheck",aligned?L"pass":L"pending");
        auto token=Read(L"Testing",L"Snapshot");if(!token.empty()&&token!=snapshotToken){snapshotToken=token;DumpGeometry(g);}
    }
    void FastLayout() noexcept {
        if(!active||inFastLayout)return;
        inFastLayout=true;
        try {
            auto g=weakGrid.get();if(g) {
                // The native list still updates its compact slots. Its theme
                // transitions animate those slots independently of Translation,
                // causing unrelated pins to slide before returning to our cells.
                // Leave native control storyboards (folder hint/open) untouched.
                SuppressLayoutTransitions(transitions,g,wc::ItemsControl::ItemContainerTransitionsProperty());
                SuppressLayoutTransitions(transitions,g,wx::UIElement::TransitionsProperty());
                if(auto panel=g.ItemsPanelRoot())SuppressLayoutTransitions(transitions,panel,wx::UIElement::TransitionsProperty());
                std::vector<uintptr_t> current;
                for(unsigned i=0;i<g.Items().Size();i++)if(auto view=g.ContainerFromIndex(int(i)).try_as<wc::GridViewItem>()) {
                    auto id=reinterpret_cast<uintptr_t>(winrt::get_abi(view));current.push_back(id);
                    auto existing=items.find(id);
                    if(existing==items.end()||existing->second->model!=g.Items().GetAt(i))Bind(view,int(i));
                    if(auto found=items.find(id);found!=items.end()) {
                        auto p=found->second;
                        SuppressLayoutTransitions(p->transitions,view,wx::UIElement::TransitionsProperty());
                        SuppressLayoutTransitions(p->transitions,view,wc::ContentControl::ContentTransitionsProperty());
                        UpdateAnchor(p,g);Position(p,p->x,p->y);
                    }
                }
                for(auto it=items.begin();it!=items.end();)if(std::find(current.begin(),current.end(),it->first)==current.end()){Detach(it->second);it=items.erase(it);arranged=false;}else ++it;
            }
        }catch(...){Error(L"Fast layout");}
        inFastLayout=false;
    }
    void CollapseSingles() {
        if(folderPending||GetTickCount64()-lastSingletonCheck<350)return;
        lastSingletonCheck=GetTickCount64();
        for(auto const& p:Ordered())if(nativefolders::Query(p->model,nativefolders::Folder)) {
            auto children=nativefolders::Children(p->model);if(children.size()!=1)continue;
            auto childId=nativefolders::Id(children.front());if(childId.empty())continue;
            auto vm=FolderViewModel();if(!vm)return;
            beforeFolder.clear();for(auto const& other:Ordered())beforeFolder.push_back(other->key);
            folderCell=p->cell;folderSource=p->key;folderTarget=p->key;replacementKey=Hash(L"native-id:"+childId);
            folderPending=true;folderAppend=false;folderCollapse=true;folderDeadline=GetTickCount64()+5000;
            placements[replacementKey]=folderCell;
            try {
                nativefolders::Close(p->model);nativefolders::MoveOut(p->model,childId);
                if(nativefolders::Children(p->model).empty()) {
                    auto folderId=nativefolders::Id(p->model);bool stillPresent=false;
                    for(auto const& child:nativefolders::Children(vm))stillPresent|=nativefolders::Id(child)==folderId;
                    if(stillPresent)nativefolders::RemoveEmpty(vm,folderId);
                }
                arranged=false;FastLayout();Log(L"Single-member folder replaced at cell="+std::to_wstring(folderCell.column)+L","+std::to_wstring(folderCell.row));
            }catch(...) {folderPending=false;Error(L"Collapse single-member folder");}
            return;
        }
    }
    void RepairFolderDuplicates() {
        if(folderPending||Number(L"RepairFolderDuplicates",0)!=1||GetTickCount64()<duplicateRepairAfter||(GetAsyncKeyState(VK_LBUTTON)&0x8000))return;
        duplicateRepairAfter=GetTickCount64()+1500;
        try {
            auto vm=FolderViewModel();if(!vm)return;
            auto roots=nativefolders::Children(vm);
            for(auto const& source:roots)if(nativefolders::Query(source,nativefolders::Tile)) {
                auto id=nativefolders::Id(source);if(id.empty())continue;
                wf::IInspectable owner{nullptr};size_t owners=0;
                for(auto const& folder:roots)if(nativefolders::Query(folder,nativefolders::Folder)&&nativefolders::Count(folder,id)>0){owner=folder;++owners;}
                if(owners!=1)continue;
                if(nativefolders::MoveInto(vm,owner,source)) {
                    auto key=Hash(L"native-id:"+id);std::wstring label=key;
                    for(auto const& p:Ordered())if(p->key==key){label=p->label;break;}
                    Log(L"Repaired root/folder duplicate by exact native ID: "+label+L" key="+key);
                    Runtime(L"DuplicateRepair",L"repaired");arranged=false;FastLayout();return;
                }
            }
            WritePrivateProfileStringW(L"Settings",L"RepairFolderDuplicates",L"0",IniPath().c_str());
            Log(L"Root/folder duplicate audit completed");
        }catch(...) {
            WritePrivateProfileStringW(L"Settings",L"RepairFolderDuplicates",L"0",IniPath().c_str());
            Runtime(L"DuplicateRepair",L"failed");Error(L"Repair folder duplicate");
        }
    }
    void DiagnosticFolderMove() {
        auto request=Read(L"Testing",L"FolderMove");if(request.empty()||folderPending)return;
        WritePrivateProfileStringW(L"Testing",L"FolderMove",nullptr,IniPath().c_str());
        auto comma=request.find(L',');if(comma==std::wstring::npos)return;
        std::shared_ptr<Item> source,target;
        for(auto const& p:Ordered()){if(p->key==request.substr(0,comma))source=p;if(p->key==request.substr(comma+1))target=p;}
        if(source&&target&&source!=target)Group(source,target);
    }
    void Tick() noexcept {
        try {
            auto g=weakGrid.get();if(!g){Restore();return;}
            if(!logged){logged=true;Status(L"inspected");Log(L"PinnedList found: "+std::to_wstring(g.Items().Size())+L" items");DumpGeometry(g);}
            if(Read(L"Settings",L"Mode",L"inspect")!=L"enable"){if(active)Restore();else Status(L"inspected");return;}
            if(!active){if(g.ActualWidth()>100&&g.Items().Size()>0)Enable(g);return;}
            SyncInput(g);
            ApplySpace(g);
            FastLayout();
            for(unsigned i=0;i<g.Items().Size();i++)if(auto view=g.ContainerFromIndex(int(i)).try_as<wc::GridViewItem>())Bind(view,int(i));
            if(items.size()!=g.Items().Size()){Status(L"waiting-containers");return;}
            FinishFolder();
            if(!folderPending&&!arranged)Arrange();
            for(auto const& p:Ordered())if(auto view=p->view.get()) {
                if(p->gesture.Active()&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)) {
                    if(p->gesture.End()==input::Release::Drop){Drop(p);view.ReleasePointerCaptures();}
                }
                UpdateAnchor(p,g);Position(p,p->x,p->y);
            }
            if(!folderPending){DiagnosticFolderMove();RepairFolderDuplicates();CollapseSingles();}
            SyncInput(g);Check(g);Status(inputMode==2?L"active-free":(inputMode==1?L"active-edit":L"active"));
        }catch(...){Error(L"Tick");Restore();WritePrivateProfileStringW(L"Settings",L"Mode",L"inspect",IniPath().c_str());Status(L"error-restored");}
    }
};

thread_local std::vector<std::shared_ptr<Controller>> t_controllers;
thread_local wx::DispatcherTimer t_timer{nullptr};
void Observe(wc::GridView const& grid) {
    for(auto const& c:t_controllers) if(c->weakGrid.get()==grid)return;
    t_controllers.push_back(std::make_shared<Controller>(grid));
    if(!t_timer) {
        t_timer=wx::DispatcherTimer();t_timer.Interval(std::chrono::milliseconds(300));
        t_timer.Tick([](auto const&,auto const&) {for(auto const& c:t_controllers)c->Tick();});t_timer.Start();
    }
}
struct Watcher:winrt::implements<Watcher,IVisualTreeServiceCallback2,winrt::non_agile> {
    winrt::com_ptr<IXamlDiagnostics> diagnostics;
    winrt::com_ptr<IVisualTreeService> service;
    explicit Watcher(IUnknown* site) {winrt::com_ptr<IUnknown> s;s.copy_from(site);diagnostics=s.as<IXamlDiagnostics>();service=s.as<IVisualTreeService>();}
    HRESULT STDMETHODCALLTYPE OnVisualTreeChange(ParentChildRelation,VisualElement e,VisualMutationType mutation) noexcept override {
        try {
            if(mutation!=Add)return S_OK;
            wf::IInspectable obj{nullptr};winrt::check_hresult(diagnostics->GetIInspectableFromHandle(e.Handle,reinterpret_cast<::IInspectable**>(winrt::put_abi(obj))));
            auto fe=obj.try_as<wx::FrameworkElement>();if(!fe)return S_OK;
            if(auto grid=fe.try_as<wc::GridView>();grid && fe.Name()==L"PinnedList")Observe(grid);
            if(auto grid=fe.try_as<wc::GridView>())Log(L"GridView discovered: "+std::wstring(fe.Name().c_str()));
        } catch (...) {Error(L"Observe");}
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnElementStateChanged(InstanceHandle,VisualElementState,LPCWSTR) noexcept override{return S_OK;}
};
winrt::com_ptr<Watcher> g_watcher;
std::atomic<bool> g_attached{};
struct Site:winrt::implements<Site,IObjectWithSite,winrt::non_agile> {
    winrt::com_ptr<IUnknown> site;
    HRESULT STDMETHODCALLTYPE SetSite(IUnknown* value) noexcept override {
        try {
            if(!value)return E_INVALIDARG;
            if(g_attached.exchange(true))return S_OK;
            site.copy_from(value);g_watcher=winrt::make_self<Watcher>(value);
            HMODULE pin{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&g_attached),&pin);
            auto handle=CreateThread(nullptr,0,[](void*)->DWORD {
                try {winrt::init_apartment(winrt::apartment_type::multi_threaded);Log(L"XAML diagnostics attached");g_watcher->service->AdviseVisualTreeChange(g_watcher.get());}
                catch (...){Error(L"AdviseVisualTreeChange");Status(L"diagnostics-failed");}return 0;
            },nullptr,0,nullptr);
            if(handle)CloseHandle(handle);else return HRESULT_FROM_WIN32(GetLastError());
            return S_OK;
        } catch (...){Error(L"SetSite");return winrt::to_hresult();}
    }
    HRESULT STDMETHODCALLTYPE GetSite(REFIID iid,void** out) noexcept override{return site.as(iid,out);}
};
struct Factory:winrt::implements<Factory,IClassFactory> {
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID iid,void** out) noexcept override {
        if(!out)return E_POINTER;*out=nullptr;if(outer)return CLASS_E_NOAGGREGATION;
        try{return winrt::make<Site>().as(iid,out);}catch(...){return winrt::to_hresult();}
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL) noexcept override{return S_OK;}
};
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID clsid,REFIID iid,void** out) {
    if(!out)return E_POINTER;*out=nullptr;if(clsid!=kClassId)return CLASS_E_CLASSNOTAVAILABLE;
    try{return winrt::make<Factory>().as(iid,out);}catch(...){return winrt::to_hresult();}
}
extern "C" HRESULT WINAPI DllCanUnloadNow(){return S_FALSE;}
extern "C" HRESULT WINAPI AttachToStart(DWORD processId) {
    auto xaml=LoadLibraryExW(L"Windows.UI.Xaml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!xaml)return HRESULT_FROM_WIN32(GetLastError());
    using Initialize=HRESULT(WINAPI*)(LPCWSTR,DWORD,LPCWSTR,LPCWSTR,CLSID,LPCWSTR);
    auto initialize=reinterpret_cast<Initialize>(GetProcAddress(xaml,"InitializeXamlDiagnosticsEx"));
    if(!initialize)return HRESULT_FROM_WIN32(GetLastError());
    wchar_t path[32768];if(!GetModuleFileNameW(g_module,path,32768))return HRESULT_FROM_WIN32(GetLastError());
    HRESULT result=HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    for(int i=1;i<=10000;++i){auto name=L"VisualDiagConnection"+std::to_wstring(i);result=initialize(name.c_str(),processId,L"",path,kClassId,nullptr);if(result!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND))break;}
    return result;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*) {
    // The host creates threads with small stack reservations. A 32K wchar
    // local here reserves 64KB even for DLL_THREAD_ATTACH and can overflow
    // before the reason check. Static TLS also means DisableThreadLibraryCalls
    // cannot be relied on to eliminate those notifications.
    if(reason!=DLL_PROCESS_ATTACH)return TRUE;
    g_module=module;
    auto buffer=static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(),0,32768*sizeof(wchar_t)));
    if(!buffer)return FALSE;
    DWORD count=GetModuleFileNameW(module,buffer,32768);BOOL result=FALSE;
    if(count>0&&count<32768)try {
        g_dir.assign(buffer,count);auto slash=g_dir.find_last_of(L"\\/");
        if(slash!=std::wstring::npos){g_dir.resize(slash);result=TRUE;}
    }catch(...){}
    HeapFree(GetProcessHeap(),0,buffer);
    return result;
}
