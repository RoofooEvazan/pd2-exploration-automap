// Read-only access to tables through the game's already mounted archives.
// Storm owns file resolution, including -direct overrides; no MPQ is mounted
// here and no game data or third-party archive library is redistributed.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>

namespace exploration {
struct GameFiles {
    using Open=BOOL(__stdcall*)(const char*,HANDLE*);
    using OpenArchive=BOOL(__stdcall*)(HANDLE,const char*,DWORD,HANDLE*);
    using Size=DWORD(__stdcall*)(HANDLE,DWORD*);
    using Read=BOOL(__stdcall*)(HANDLE,void*,DWORD,DWORD*,OVERLAPPED*);
    using Close=BOOL(__stdcall*)(HANDLE);
    Open open=nullptr;
    Size size=nullptr;
    Read read=nullptr;
    Close close=nullptr;
    OpenArchive openArchive=nullptr;
    bool archiveOnly=false; // Explicit table diagnostic; never changes game resolution.
    explicit operator bool() const {return open && size && read && close && (!archiveOnly || openArchive);}
    // Bind only the inspected x86 Storm build. Never load a second Storm copy.
    static GameFiles bind(HMODULE module,decltype(&GetProcAddress) resolve=GetProcAddress) {
        __try {
            if(!module)return {};
            auto base=reinterpret_cast<const unsigned char*>(module);
            auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>4096)return {};
            auto nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(base+dos->e_lfanew);
            if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
               nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
               nt->OptionalHeader.SizeOfImage!=0x60000 || nt->FileHeader.TimeDateStamp!=0x4b95c049)return {};
            auto open=resolve(module,MAKEINTRESOURCEA(267));
            auto openEx=resolve(module,MAKEINTRESOURCEA(268));
            auto size=resolve(module,MAKEINTRESOURCEA(265));
            auto read=resolve(module,MAKEINTRESOURCEA(269));
            auto close=resolve(module,MAKEINTRESOURCEA(253));
            if(reinterpret_cast<const unsigned char*>(open)!=base+0x28da0 ||
               reinterpret_cast<const unsigned char*>(openEx)!=base+0x28960 ||
               reinterpret_cast<const unsigned char*>(size)!=base+0x262d0 ||
               reinterpret_cast<const unsigned char*>(read)!=base+0x29be0 ||
               reinterpret_cast<const unsigned char*>(close)!=base+0x26e20)return {};
            // Instruction bytes avoid relocated immediates and imported addresses.
            const unsigned char openBytes[]={0x8b,0x4c,0x24,0x10,0x81,0xec,0x10,0x01,0x00,0x00};
            const unsigned char resolverBytes[]={0x33,0xc0,0xf6,0xc2,0x01,0x74,0x05,0xb8,0x01,0x00,0x00,0x00,
                0xf6,0xc2,0x02,0x74,0x03,0x83,0xc8,0x02};
            const unsigned char resolverCall[]={0x6a,0x00,0xe8,0x81,0xfb,0xff,0xff,0xc2,0x08,0x00};
            const unsigned char sizeBytes[]={0x56,0x57,0x8b,0x7c,0x24,0x10,0x85,0xff};
            const unsigned char readBytes[]={0x8b,0x44,0x24,0x14,0x8b,0x4c,0x24,0x10,0x8b,0x54,0x24,0x0c};
            const unsigned char closeBytes[]={0x8b,0x7c,0x24,0x08,0x85,0xff};
            if(base[0x28da0]!=0x8b || base[0x28da1]!=0x15 ||
               *reinterpret_cast<const uintptr_t*>(base+0x28da2)!=reinterpret_cast<uintptr_t>(base+0x53130) ||
               memcmp(base+0x28da6,resolverBytes,sizeof(resolverBytes)) ||
               memcmp(base+0x28dd8,resolverCall,sizeof(resolverCall)) ||
               memcmp(base+0x28960,openBytes,sizeof(openBytes)) ||
               memcmp(base+0x262d0,sizeBytes,sizeof(sizeBytes)) ||
               memcmp(base+0x29be0,readBytes,sizeof(readBytes)) ||
               memcmp(base+0x26e2c,closeBytes,sizeof(closeBytes)))return {};
            return {reinterpret_cast<Open>(open),reinterpret_cast<Size>(size),
                reinterpret_cast<Read>(read),reinterpret_cast<Close>(close),reinterpret_cast<OpenArchive>(openEx)};
        } __except(EXCEPTION_EXECUTE_HANDLER){return {};}
    }
};

enum class TableRead { Ready, Unavailable, OpenFailed, InvalidSize, ReadFailed };
inline const char* tableReadName(TableRead status) {
    switch(status) {
    case TableRead::Ready:return "ready";
    case TableRead::Unavailable:return "unsupported Storm reader";
    case TableRead::OpenFailed:return "not found in active game files";
    case TableRead::InvalidSize:return "empty, oversized or invalid size";
    case TableRead::ReadFailed:return "failed or incomplete read";
    }
    return "unknown";
}
inline TableRead readGameTable(const GameFiles& api,const char* path,std::string& bytes) {
    bytes.clear();
    if(!api)return TableRead::Unavailable;
    HANDLE file=nullptr;
    // SFileOpenFile consults Storm's current direct-access setting before
    // delegating to its mounted archive search. OpenFileEx with a hardcoded
    // scope 0 would incorrectly ignore the user's -direct overrides.
    const bool opened=api.archiveOnly?api.openArchive(nullptr,path,0,&file)!=FALSE:api.open(path,&file)!=FALSE;
    if(!opened)return TableRead::OpenFailed;
    if(!file || file==INVALID_HANDLE_VALUE)return TableRead::OpenFailed;
    struct Owner {HANDLE file;GameFiles::Close close;~Owner(){close(file);}} owner{file,api.close};
    DWORD high=0,length=api.size(file,&high);
    if(high || !length || length>16u*1024u*1024u)return TableRead::InvalidSize;
    bytes.resize(length);
    DWORD received=0;
    if(!api.read(file,bytes.data(),length,&received,nullptr) || received!=length) {
        bytes.clear();return TableRead::ReadFailed;
    }
    return TableRead::Ready;
}
}
