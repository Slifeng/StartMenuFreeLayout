#pragma once
#include <unknwn.h>
#include <string>
#include <vector>
#include <winrt/Windows.Foundation.h>
// Private WinRT ABI verified against Microsoft's matching public PDB:
// Native ABI is pinned to the component fingerprint verified by the launcher.
// Use QueryInterface and COM ABI only; never call implementation addresses.
namespace nativefolders {
namespace wf=winrt::Windows::Foundation;
inline constexpr GUID List{0x3fe152a0,0x3235,0x5435,{0x92,0x01,0x56,0xd5,0xea,0xb2,0xfb,0xb6}};
inline constexpr GUID ListModel{0xfb0ceff1,0xe151,0x5915,{0x8f,0x00,0x39,0xc1,0x4e,0x95,0x02,0xe7}};
inline constexpr GUID Tile{0xf6dac573,0x6855,0x5e36,{0xa8,0x73,0xd6,0x93,0xee,0x88,0x01,0x22}};
inline constexpr GUID Folder{0x350e7f51,0xeb4e,0x595d,{0x8d,0x28,0x95,0x53,0x23,0xc8,0xd3,0x50}};
inline constexpr GUID Item{0x150b82ec,0x1f64,0x5a8f,{0x85,0x97,0x63,0x41,0x42,0x5b,0x9f,0x6b}};
inline constexpr GUID Container{0x5b0748e2,0xef06,0x52af,{0x9d,0x15,0x3d,0x83,0x85,0xae,0x0f,0x91}};
inline constexpr GUID ItemVector{0x844e624e,0xd7f5,0x5044,{0x87,0x62,0x8d,0x2d,0x5f,0x42,0xa5,0xc3}};
inline bool Compatible() {
    auto module=GetModuleHandleW(L"StartMenu.dll");if(!module)return false;
    auto base=reinterpret_cast<unsigned char const*>(module);
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER const*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return false;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 const*>(base+dos->e_lfanew);
    return pe->Signature==IMAGE_NT_SIGNATURE&&pe->FileHeader.TimeDateStamp==0x6970337c&&pe->OptionalHeader.SizeOfImage==4841472;
}
inline winrt::com_ptr<IUnknown> Query(wf::IInspectable const& object,GUID const& iid) {
    winrt::com_ptr<IUnknown> result;
    if(object)reinterpret_cast<IUnknown*>(winrt::get_abi(object))->QueryInterface(iid,result.put_void());
    return result;
}
template<class Function> Function Method(IUnknown* object,size_t slot) {
    return reinterpret_cast<Function>((*reinterpret_cast<void***>(object))[slot]);
}
inline std::wstring Id(wf::IInspectable const& object) {
    auto item=Query(object,Item);if(!item)return {};
    using Getter=HRESULT(WINAPI*)(void*,void**);winrt::hstring value;
    winrt::check_hresult(Method<Getter>(item.get(),6)(item.get(),winrt::put_abi(value)));
    return value.c_str();
}
inline wf::IInspectable ViewModel(wf::IInspectable const& control) {
    if(Query(control,ListModel))return control;
    auto list=Query(control,List);if(!list)return nullptr;
    using Getter=HRESULT(WINAPI*)(void*,void**);wf::IInspectable result{nullptr};
    winrt::check_hresult(Method<Getter>(list.get(),6)(list.get(),winrt::put_abi(result)));
    return result;
}
inline void Create(wf::IInspectable const& viewModel,wf::IInspectable const& source,wf::IInspectable const& target) {
    auto list=Query(viewModel,ListModel),first=Query(source,Tile),second=Query(target,Tile);
    if(!list||!first||!second)throw winrt::hresult_error(E_NOINTERFACE,L"原生建文件夹接口不适用于此项目。");
    using CreateFolder=HRESULT(WINAPI*)(void*,void*,void*);
    winrt::check_hresult(Method<CreateFolder>(list.get(),17)(list.get(),first.get(),second.get()));
}
inline void Append(wf::IInspectable const& folder,wf::IInspectable const& source) {
    auto container=Query(folder,Container),tile=Query(source,Tile);
    if(!Query(folder,Folder)||!container||!tile)throw winrt::hresult_error(E_NOINTERFACE,L"此项目不支持加入原生文件夹。");
    using AppendTile=HRESULT(WINAPI*)(void*,void*);
    winrt::check_hresult(Method<AppendTile>(container.get(),16)(container.get(),tile.get()));
}
inline std::vector<wf::IInspectable> Children(wf::IInspectable const& folder) {
    std::vector<wf::IInspectable> result;
    auto container=Query(folder,Container);if(!container)return result;
    using Getter=HRESULT(WINAPI*)(void*,void**);wf::IInspectable collection{nullptr};
    winrt::check_hresult(Method<Getter>(container.get(),7)(container.get(),winrt::put_abi(collection)));
    auto vector=Query(collection,ItemVector);if(!vector)return result;
    using Size=HRESULT(WINAPI*)(void*,uint32_t*);uint32_t count{};
    winrt::check_hresult(Method<Size>(vector.get(),7)(vector.get(),&count));
    if(count>256)throw winrt::hresult_error(E_BOUNDS);
    using GetAt=HRESULT(WINAPI*)(void*,uint32_t,void**);
    for(uint32_t i=0;i<count;i++) {
        wf::IInspectable item{nullptr};winrt::check_hresult(Method<GetAt>(vector.get(),6)(vector.get(),i,winrt::put_abi(item)));result.push_back(item);
    }
    return result;
}
inline void Close(wf::IInspectable const& folder) {
    auto object=Query(folder,Folder);if(!object)return;
    using Setter=HRESULT(WINAPI*)(void*,bool);
    winrt::check_hresult(Method<Setter>(object.get(),9)(object.get(),false));
}
inline void MoveOut(wf::IInspectable const& folder,std::wstring const& id) {
    auto object=Query(folder,Folder);if(!object)throw winrt::hresult_error(E_NOINTERFACE);
    using Move=HRESULT(WINAPI*)(void*,void*);winrt::hstring value(id);
    winrt::check_hresult(Method<Move>(object.get(),6)(object.get(),winrt::get_abi(value)));
}
inline void Remove(wf::IInspectable const& viewModel,std::wstring const& id) {
    auto object=Query(viewModel,Container);if(!object)throw winrt::hresult_error(E_NOINTERFACE);
    using Remove=HRESULT(WINAPI*)(void*,void*);winrt::hstring value(id);
    winrt::check_hresult(Method<Remove>(object.get(),17)(object.get(),winrt::get_abi(value)));
}
inline void RemoveEmpty(wf::IInspectable const& viewModel,std::wstring const& id) { Remove(viewModel,id); }
inline size_t Count(wf::IInspectable const& container,std::wstring const& id) {
    if(id.empty())return 0;
    size_t count=0;for(auto const& child:Children(container))if(Id(child)==id)++count;return count;
}
// AppendPinToContainer only inserts into the destination. Moving a pin requires
// removing it from its original container after verifying destination ownership.
// Never match display names, and never remove the source if insertion failed.
inline bool MoveInto(wf::IInspectable const& root,wf::IInspectable const& folder,wf::IInspectable const& source) {
    auto id=Id(source);
    if(id.empty()||!Query(source,Tile)||!Query(folder,Folder)||!Query(root,Container))throw winrt::hresult_error(E_NOINTERFACE);
    auto roots=Count(root,id),members=Count(folder,id);
    if(roots==0)return false; // repeated completion must not add another copy
    if(roots!=1||members>1)throw winrt::hresult_error(E_UNEXPECTED,L"固定项 ID 不唯一，未更改原位置。");
    if(members==0)Append(folder,source);
    if(Count(folder,id)!=1)throw winrt::hresult_error(E_FAIL,L"未确认目标文件夹含有此应用，已保留外层图标。");
    Remove(root,id);
    if(Count(root,id)!=0||Count(folder,id)!=1)throw winrt::hresult_error(E_FAIL,L"文件夹移动结果未通过核对。");
    return true;
}
}
