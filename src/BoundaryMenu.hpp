// Optional extension of the supported PD2 Automap Options menu. The native
// menu owns navigation, hit testing and resources; only our row uses live text.
// All edits are in process memory, validated and installed on the game thread.
#pragma once
#include <windows.h>
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <iterator>
#include "BoundaryColors.hpp"
namespace exploration::boundary_menu {
struct Entry;
using Callback=BOOL(__fastcall*)(Entry*,void*);
struct Entry {
    DWORD type,expansion,y;
    char artwork[260];
    Callback enabled,press,initialize,refresh;
    DWORD choices,value,bar;
    char switchArtwork[4][260];
    void* cell;
    void* switches[4];
};
struct Menu { DWORD count,spacing,textHeight,offset,barHeight,unused; };
static_assert(sizeof(Entry)==0x550 && offsetof(Entry,cell)==0x53c,"native menu ABI");
static_assert(sizeof(Menu)==0x18,"native menu descriptor ABI");
using CellText=void(__fastcall*)(void*,int,int,int,int,int);
using DrawText=void(__fastcall*)(const wchar_t*,int,int,DWORD,DWORD);
using TextSize=DWORD(__fastcall*)(DWORD);
using TextWidth=void(__fastcall*)(const wchar_t*,DWORD*,DWORD*);
using SelectColor=bool(*)(BoundaryColor);
using CurrentColor=BoundaryColor(*)();
inline std::array<Entry,10> entries{};
inline Menu menu{};
inline Entry** activeEntries=nullptr;
inline CellText originalText=nullptr;
inline DrawText drawText=nullptr;
inline TextSize textSize=nullptr;
inline TextWidth textWidth=nullptr;
inline SelectColor selectColor=nullptr;
inline CurrentColor currentColor=nullptr;
inline bool installed=false,saveFailed=false;
inline BOOL __fastcall refresh(Entry* row,void*) {
    row->value=static_cast<unsigned>(currentColor());return TRUE;
}
inline BOOL __fastcall press(Entry* row,void*) {
    const auto next=static_cast<BoundaryColor>((static_cast<unsigned>(currentColor())+1)%boundaryPresets.size());
    saveFailed=!selectColor(next);refresh(row,nullptr);return TRUE;
}
inline void makeMenu(const Menu& source,const Entry* sourceEntries) {
    menu=source;menu.count=10;
    std::copy_n(sourceEntries,8,entries.begin());
    entries[8]=Entry{};entries[8].type=0;
    entries[8].press=press;entries[8].initialize=refresh;entries[8].refresh=refresh;
    entries[9]=sourceEntries[8];
}
inline void __fastcall drawRow(void* cell,int x,int y,int align,int mode,int extra) {
    // Our type-0 row deliberately has no DC6 resource, so normal menu teardown
    // has nothing extra to release. The label uses the game's existing font.
    if(cell || !activeEntries || *activeEntries!=entries.data() ||
       y!=int(entries[8].y+menu.textHeight)) {
        originalText(cell,x,y,align,mode,extra);return;
    }
    wchar_t label[96]{};
    swprintf_s(label,L"Boundary Color: %s",boundaryPreset(currentColor()).label);
    auto old=textSize(7); // Native Font24, matching the options' scale.
    DWORD width=0,file=0;textWidth(label,&width,&file);
    drawText(label,x-int(width)/2,y,4,0); // Native gold, consistent with options.
    if(saveFailed) {
        textSize(0);
        constexpr auto error=L"Could not save boundary color";
        textWidth(error,&width,&file);drawText(error,x-int(width)/2,y+11,1,0);
    }
    textSize(old);
}
template<class T> inline T at(const unsigned char* p,std::size_t offset) {
    T value;std::memcpy(&value,p+offset,sizeof(value));return value;
}
inline bool profile(const unsigned char* p,DWORD size,DWORD stamp) {
    if(!p || at<WORD>(p,0)!=IMAGE_DOS_SIGNATURE)return false;
    auto pe=at<DWORD>(p,0x3c);if(pe<0x40 || pe>0x1000)return false;
    auto n=reinterpret_cast<const IMAGE_NT_HEADERS32*>(p+pe);
    return n->Signature==IMAGE_NT_SIGNATURE && n->FileHeader.Machine==IMAGE_FILE_MACHINE_I386 &&
        n->FileHeader.TimeDateStamp==stamp && n->OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
        n->OptionalHeader.SizeOfImage==size;
}
// No speculative scans or offsets for unrecognized PD2 versions.
inline bool compatible(const unsigned char* pd,const unsigned char* client) {
    if(!profile(pd,0x536000,0x6a049b81) || !profile(client,0x135000,0x4b95ca3e))return false;
    const auto source=reinterpret_cast<const Menu*>(pd+0x39da90);
    if(source->count!=9 || source->spacing!=45 || source->textHeight!=34 || source->offset!=49)return false;
    const auto rows=reinterpret_cast<const Entry*>(pd+0x3a3fb0);
    if(std::strcmp(rows[0].artwork,"AutoMapOptions") || std::strcmp(rows[8].artwork,"SPrevious") ||
       rows[0].type!=0xffffffff || rows[8].type!=0)return false;
    for(auto offset:{0x22fc79,0x22fe44,0x230410})
        if(at<const void*>(pd,offset)!=source)return false;
    for(auto offset:{0x22fc7e,0x22fe3f,0x230416})
        if(at<const void*>(pd,offset)!=rows)return false;
    if(client[0x653ae]!=0xe8 || client+0x653b3+at<int>(client,0x653af)!=client+0xd372)return false;
    // The cell renderer must still receive x/y and four stack arguments.
    const unsigned char before[]={0x6a,0x01,0x8b,0xd0,0x53,0xd1,0xfa};
    if(std::memcmp(client+0x653a7,before,sizeof(before)))return false;
    if(client[0x65395]!=0xa1 || at<const void*>(client,0x65396)!=client+0xdbc48 ||
       client[0x65300]!=0xa1 || at<const void*>(client,0x65301)!=client+0x11c060)return false;
    return true;
}
inline bool install(unsigned char* pd,unsigned char* client,unsigned char* win,SelectColor select,CurrentColor current) {
    if(installed)return true;
    if(!client || !win || !compatible(pd,client) || !profile(win,0xcf000,0x4b95c21d))return false;
    const auto d=GetProcAddress(reinterpret_cast<HMODULE>(win),MAKEINTRESOURCEA(10150));
    const auto s=GetProcAddress(reinterpret_cast<HMODULE>(win),MAKEINTRESOURCEA(10184));
    const auto w=GetProcAddress(reinterpret_cast<HMODULE>(win),MAKEINTRESOURCEA(10177));
    if(reinterpret_cast<unsigned char*>(d)!=win+0x12fa0 || reinterpret_cast<unsigned char*>(s)!=win+0x12fe0 ||
       reinterpret_cast<unsigned char*>(w)!=win+0x12700)return false;
    struct Patch { unsigned char* site;DWORD value,protection; };
    const auto descriptor=reinterpret_cast<DWORD>(&menu),table=reinterpret_cast<DWORD>(entries.data());
    Patch patches[]={{pd+0x22fc79,descriptor,0},{pd+0x22fe44,descriptor,0},{pd+0x230410,descriptor,0},
        {pd+0x22fc7e,table,0},{pd+0x22fe3f,table,0},{pd+0x230416,table,0},
        {client+0x653af,DWORD(reinterpret_cast<uintptr_t>(drawRow)-reinterpret_cast<uintptr_t>(client+0x653b3)),0}};
    std::size_t ready=0;
    for(auto& patch:patches) {
        if(!VirtualProtect(patch.site,4,PAGE_EXECUTE_READWRITE,&patch.protection))break;
        ++ready;
    }
    if(ready==std::size(patches)) {
        selectColor=select;currentColor=current;
        makeMenu(*reinterpret_cast<Menu*>(pd+0x39da90),reinterpret_cast<Entry*>(pd+0x3a3fb0));
        originalText=reinterpret_cast<CellText>(client+0xd372);
        drawText=reinterpret_cast<DrawText>(d);textSize=reinterpret_cast<TextSize>(s);textWidth=reinterpret_cast<TextWidth>(w);
        activeEntries=reinterpret_cast<Entry**>(client+0x11c060);
        // Transfer already-loaded resources if initialization happened after
        // the native menu load. All three native load/select/free references
        // then use the owned table; no second owner frees these resources.
        auto activeMenu=reinterpret_cast<Menu**>(client+0x11c05c);
        if(*activeMenu==reinterpret_cast<Menu*>(pd+0x39da90))*activeMenu=&menu;
        if(*activeEntries==reinterpret_cast<Entry*>(pd+0x3a3fb0)) {
            *activeEntries=entries.data();
            auto selection=reinterpret_cast<DWORD*>(client+0x11c058);if(*selection==8)*selection=9;
        }
        for(auto& patch:patches){std::memcpy(patch.site,&patch.value,4);FlushInstructionCache(GetCurrentProcess(),patch.site,4);}
        installed=true;
    }
    while(ready){auto& patch=patches[--ready];DWORD ignored;VirtualProtect(patch.site,4,patch.protection,&ignored);}
    return installed;
}
}
