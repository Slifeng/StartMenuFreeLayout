#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
DWORD WINAPI Worker(void*) {return 17;}
int wmain(int argc,wchar_t** argv) {
    if(argc!=2)return 2;
    SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS);
    auto module=LoadLibraryW(argv[1]);if(!module){std::cout<<"Load failed "<<GetLastError()<<"\n";return 3;}
    for(int i=0;i<24;i++) {
        HANDLE thread=CreateThread(nullptr,64*1024,Worker,nullptr,STACK_SIZE_PARAM_IS_A_RESERVATION,nullptr);
        if(!thread)return 4;
        if(WaitForSingleObject(thread,10000)!=WAIT_OBJECT_0)return 5;
        DWORD code{};GetExitCodeThread(thread,&code);CloseHandle(thread);
        if(code!=17)return 6;
    }
    std::cout<<"PASS: DLL load and 24 thread attach/detach cycles on 64KB thread stacks\n";
    FreeLibrary(module);return 0;
}
