#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winstring.h>
#include <winrt/Windows.Foundation.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "NativeFolders.h"
struct Fake {
    void** table;
    GUID iid;
    ULONG refs=1;
    std::vector<Fake*> linked;
    Fake* value{};
    std::vector<Fake*> elements;
    static HRESULT WINAPI Query(void* raw,REFIID iid,void** output) {
        auto self=static_cast<Fake*>(raw);*output=nullptr;
        if(iid==self->iid){*output=self;self->refs++;return S_OK;}
        for(auto other:self->linked)if(iid==other->iid){*output=other;other->refs++;return S_OK;}
        return E_NOINTERFACE;
    }
    static ULONG WINAPI Add(void* raw){return ++static_cast<Fake*>(raw)->refs;}
    static ULONG WINAPI Release(void* raw){return --static_cast<Fake*>(raw)->refs;}
};
static int created{},added{},moved{},closed{},removed{};
static void* receivedSource{};
static void* receivedTarget{};
static HRESULT WINAPI Create(void*,void* source,void* target){created++;receivedSource=source;receivedTarget=target;return S_OK;}
static HRESULT WINAPI Append(void*,void* source){added++;receivedSource=source;return S_OK;}
static HRESULT WINAPI GetVM(void* raw,void** out){auto self=static_cast<Fake*>(raw);*out=self->value;self->value->refs++;return S_OK;}
static HRESULT WINAPI GetId(void*,void** out){return WindowsCreateString(L"stable-native-pin-id",20,reinterpret_cast<HSTRING*>(out));}
static HRESULT WINAPI Size(void* raw,uint32_t* out){*out=static_cast<uint32_t>(static_cast<Fake*>(raw)->elements.size());return S_OK;}
static HRESULT WINAPI GetAt(void* raw,uint32_t index,void** out){auto self=static_cast<Fake*>(raw);if(index>=self->elements.size())return E_BOUNDS;*out=self->elements[index];self->elements[index]->refs++;return S_OK;}
static HRESULT WINAPI Close(void*,bool open){assert(!open);closed++;return S_OK;}
static HRESULT WINAPI MoveOut(void* raw,void* value){uint32_t n{};auto text=WindowsGetStringRawBuffer(static_cast<HSTRING>(value),&n);assert(std::wstring(text,n)==L"stable-native-pin-id");moved++;static_cast<Fake*>(raw)->value->elements.clear();return S_OK;}
static HRESULT WINAPI Remove(void*,void* value){uint32_t n{};auto text=WindowsGetStringRawBuffer(static_cast<HSTRING>(value),&n);assert(std::wstring(text,n)==L"stable-native-pin-id");removed++;return S_OK;}
static int transferAdds{},transferRemoves{},appendBehavior{};
static HRESULT WINAPI TransferAppend(void* raw,void* source) {
    transferAdds++;
    if(appendBehavior==1)return E_FAIL;
    if(appendBehavior!=2)static_cast<Fake*>(raw)->value->elements.push_back(static_cast<Fake*>(source));
    return S_OK;
}
static HRESULT WINAPI TransferRemove(void* raw,void* value) {
    uint32_t n{};auto text=WindowsGetStringRawBuffer(static_cast<HSTRING>(value),&n);
    assert(std::wstring(text,n)==L"stable-native-pin-id");transferRemoves++;
    static_cast<Fake*>(raw)->value->elements.clear();return S_OK;
}
static void TransferTests() {
    void *itemTable[18]{},*tileTable[18]{},*folderTable[18]{},*containerTable[18]{},*vectorTable[18]{};
    for(auto table:{itemTable,tileTable,folderTable,containerTable,vectorTable}) {
        table[0]=reinterpret_cast<void*>(&Fake::Query);table[1]=reinterpret_cast<void*>(&Fake::Add);table[2]=reinterpret_cast<void*>(&Fake::Release);
    }
    itemTable[6]=reinterpret_cast<void*>(&GetId);containerTable[7]=reinterpret_cast<void*>(&GetVM);
    containerTable[16]=reinterpret_cast<void*>(&TransferAppend);containerTable[17]=reinterpret_cast<void*>(&TransferRemove);
    vectorTable[6]=reinterpret_cast<void*>(&GetAt);vectorTable[7]=reinterpret_cast<void*>(&Size);
    Fake identity{itemTable,nativefolders::Item},source{tileTable,nativefolders::Tile},folder{folderTable,nativefolders::Folder};
    Fake root{containerTable,nativefolders::Container},destination{containerTable,nativefolders::Container};
    Fake roots{vectorTable,nativefolders::ItemVector},members{vectorTable,nativefolders::ItemVector};
    source.linked={&identity};folder.linked={&destination};root.value=&roots;destination.value=&members;roots.elements={&source};
    {
        using winrt::Windows::Foundation::IInspectable;
        IInspectable rootObject{&root,winrt::take_ownership_from_abi},folderObject{&folder,winrt::take_ownership_from_abi},sourceObject{&source,winrt::take_ownership_from_abi};
        assert(nativefolders::MoveInto(rootObject,folderObject,sourceObject));
        assert(roots.elements.empty()&&members.elements.size()==1&&transferAdds==1&&transferRemoves==1);
        assert(!nativefolders::MoveInto(rootObject,folderObject,sourceObject));
        assert(transferAdds==1&&transferRemoves==1); // repeat is idempotent
        roots.elements={&source}; // repair a pre-existing root/folder duplicate
        assert(nativefolders::MoveInto(rootObject,folderObject,sourceObject));
        assert(transferAdds==1&&transferRemoves==2&&members.elements.size()==1);
        for(int behavior:{1,2}) { // failing and no-op native insertion preserve root
            roots.elements={&source};members.elements.clear();appendBehavior=behavior;
            bool rejected=false;try{nativefolders::MoveInto(rootObject,folderObject,sourceObject);}catch(winrt::hresult_error const&){rejected=true;}
            assert(rejected&&roots.elements.size()==1&&transferRemoves==2);
        }
        appendBehavior=0;roots.elements={&source,&source};
        bool rejected=false;try{nativefolders::MoveInto(rootObject,folderObject,sourceObject);}catch(winrt::hresult_error const&){rejected=true;}
        assert(rejected&&roots.elements.size()==2&&transferRemoves==2);
    }
    assert(identity.refs==1&&destination.refs==1&&roots.refs==1&&members.refs==1);
    assert(root.refs==0&&source.refs==0&&folder.refs==0);
}
int main() {
    TransferTests();
    void *itemTable[18]{},*tileTable[18]{},*rootTable[18]{},*vmTable[18]{},*folderTable[18]{},*containerTable[18]{},*vectorTable[18]{};
    for(auto table:{itemTable,tileTable,rootTable,vmTable,folderTable,containerTable,vectorTable}) {
        table[0]=reinterpret_cast<void*>(&Fake::Query);table[1]=reinterpret_cast<void*>(&Fake::Add);table[2]=reinterpret_cast<void*>(&Fake::Release);
    }
    itemTable[6]=reinterpret_cast<void*>(&GetId);rootTable[6]=reinterpret_cast<void*>(&GetVM);
    vmTable[17]=reinterpret_cast<void*>(&Create);containerTable[16]=reinterpret_cast<void*>(&Append);
    containerTable[7]=reinterpret_cast<void*>(&GetVM);containerTable[17]=reinterpret_cast<void*>(&Remove);
    folderTable[6]=reinterpret_cast<void*>(&MoveOut);folderTable[9]=reinterpret_cast<void*>(&Close);
    vectorTable[6]=reinterpret_cast<void*>(&GetAt);vectorTable[7]=reinterpret_cast<void*>(&Size);
    Fake identity{itemTable,nativefolders::Item},source{tileTable,nativefolders::Tile},target{tileTable,nativefolders::Tile};
    Fake vm{vmTable,nativefolders::ListModel},root{rootTable,nativefolders::List},folder{folderTable,nativefolders::Folder},container{containerTable,nativefolders::Container};
    Fake vector{vectorTable,nativefolders::ItemVector};vector.elements={&source};container.value=&vector;folder.value=&vector;vm.linked={&container};
    source.linked={&identity};root.value=&vm;folder.linked={&container};
    {
        namespace wf=winrt::Windows::Foundation;
        wf::IInspectable rootObject{&root,winrt::take_ownership_from_abi},sourceObject{&source,winrt::take_ownership_from_abi},targetObject{&target,winrt::take_ownership_from_abi},folderObject{&folder,winrt::take_ownership_from_abi};
        assert(nativefolders::Id(sourceObject)==L"stable-native-pin-id");
        auto model=nativefolders::ViewModel(rootObject);
        nativefolders::Create(model,sourceObject,targetObject);assert(created==1&&receivedSource==&source&&receivedTarget==&target);
        nativefolders::Append(folderObject,sourceObject);assert(added==1&&receivedSource==&source);
        {auto members=nativefolders::Children(folderObject);assert(members.size()==1&&nativefolders::Id(members.front())==L"stable-native-pin-id");}
        nativefolders::Close(folderObject);nativefolders::MoveOut(folderObject,L"stable-native-pin-id");
        assert(closed==1&&moved==1&&nativefolders::Children(folderObject).empty());
        nativefolders::RemoveEmpty(model,L"stable-native-pin-id");assert(removed==1);
        bool rejected=false;try{nativefolders::Create(model,folderObject,targetObject);}catch(winrt::hresult_error const&){rejected=true;}
        assert(rejected&&created==1); // unsupported nesting cannot invoke creation
    }
    assert(identity.refs==1&&container.refs==1&&vm.refs==1&&vector.refs==1);
    assert(root.refs==0&&source.refs==0&&target.refs==0&&folder.refs==0);
    std::cout<<"PASS: verified transfer, duplicate repair, idempotency, failed/no-op insertion preserves source, ambiguous IDs rejected; identity, COM ownership, folder ABI\n";
}
