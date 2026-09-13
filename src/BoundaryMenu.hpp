// Optional extension of the supported PD2 Automap Options menu. The native
// menu owns navigation, hit testing and resources; our rows use live text.
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
inline std::array<Entry,11> entries{};
inline std::array<Entry,boundaryPresets.size()+2> pickerEntries{};
inline Menu menu{},pickerMenu{};
inline Entry** activeEntries=nullptr;
inline Menu** activeMenu=nullptr;
inline DWORD* selection=nullptr;
inline DWORD* escapeSelection=nullptr;
inline CellText originalText=nullptr;
inline DrawText drawText=nullptr;
inline TextSize textSize=nullptr;
inline TextWidth textWidth=nullptr;
inline SelectColor selectColor=nullptr;
inline CurrentColor currentColor=nullptr;
inline SelectColor selectWallColor=nullptr;
inline CurrentColor currentWallColor=nullptr;
inline bool installed=false,saveFailed=false;
inline bool pickingWalls=false;
inline void activate(Menu& descriptor,Entry* rows,DWORD selected) {
    *activeEntries=rows;*activeMenu=&descriptor;
    *selection=selected;*escapeSelection=descriptor.count-1;
}
inline BOOL __fastcall back(Entry*,void*) {
    if(!activeEntries || !activeMenu || !selection || !escapeSelection ||
       *activeEntries!=pickerEntries.data() || *activeMenu!=&pickerMenu)return FALSE;
    activate(menu,entries.data(),pickingWalls?9:8);saveFailed=false;return TRUE;
}
inline BOOL __fastcall choose(Entry* row,void*) {
    if(!activeEntries || *activeEntries!=pickerEntries.data())return FALSE;
    for(unsigned i=0;i<boundaryPresets.size();++i)if(row==&pickerEntries[i+1]) {
        const auto color=static_cast<BoundaryColor>(i);
        saveFailed=!(pickingWalls?selectWallColor:selectColor)(color);
        if(!saveFailed)back(nullptr,nullptr);
        return TRUE;
    }
    return FALSE;
}
inline BOOL openPicker(bool walls) {
    if(!activeEntries || !activeMenu || !selection || !escapeSelection ||
       *activeEntries!=entries.data() || *activeMenu!=&menu)return FALSE;
    pickingWalls=walls;saveFailed=false;
    auto color=static_cast<unsigned>((walls?currentWallColor:currentColor)());
    if(color>=boundaryPresets.size())color=walls?static_cast<unsigned>(BoundaryColor::Gray):0;
    activate(pickerMenu,pickerEntries.data(),color+1);return TRUE;
}
inline BOOL __fastcall openBoundary(Entry*,void*) {return openPicker(false);}
inline BOOL __fastcall openWalls(Entry*,void*) {return openPicker(true);}
inline void bindActive(unsigned char* pd,unsigned char* client) {
    activeEntries=reinterpret_cast<Entry**>(client+0x11c060);
    activeMenu=reinterpret_cast<Menu**>(client+0x11c05c);
    selection=reinterpret_cast<DWORD*>(client+0x11c058);
    escapeSelection=reinterpret_cast<DWORD*>(pd+0x3d98e4);
    // The copied main table is the sole owner used by native load/free.
    // The picker has no resources and is never passed to either operation.
    if(*activeMenu==reinterpret_cast<Menu*>(pd+0x39da90) &&
       *activeEntries==reinterpret_cast<Entry*>(pd+0x3a3fb0)) {
        const auto selected=*selection==8?10:*selection;
        activate(menu,entries.data(),selected<menu.count?selected:10);
    }
}
inline void makeMenu(const Menu& source,const Entry* sourceEntries) {
    menu=source;menu.count=DWORD(entries.size());menu.spacing=36;
    std::copy_n(sourceEntries,8,entries.begin());
    entries[8]=Entry{};entries[8].press=openBoundary;
    entries[9]=Entry{};entries[9].press=openWalls;
    entries[10]=sourceEntries[8];
    pickerMenu=source;pickerMenu.count=DWORD(pickerEntries.size());
    pickerMenu.spacing=25;pickerMenu.textHeight=22;pickerMenu.barHeight=24;
    pickerEntries={};pickerEntries[0].type=0xffffffff;
    for(unsigned i=0;i<boundaryPresets.size();++i)pickerEntries[i+1].press=choose;
    pickerEntries.back().press=back;
}
inline void __fastcall drawRow(void* cell,int x,int y,int align,int mode,int extra) {
    if(cell || !activeEntries) {
        originalText(cell,x,y,align,mode,extra);return;
    }
    wchar_t label[96]{};
    DWORD font=2,color=4; // Native Font30 matches the existing option artwork.
    if(*activeEntries==entries.data()) {
        const wchar_t* value=nullptr;
        if(y==int(entries[8].y+menu.textHeight))
            {swprintf_s(label,L"Boundary Color");value=boundaryPreset(currentColor()).label;}
        else if(y==int(entries[9].y+menu.textHeight))
            {swprintf_s(label,L"Wall Color");value=boundaryPreset(currentWallColor()).label;}
        if(value) {
            // Native option labels start 230 pixels left of center; values
            // end 230 pixels right of center, on the same text baseline.
            const auto old=textSize(font);
            DWORD width=0,file=0;textWidth(value,&width,&file);
            drawText(label,x-230,y,color,0);
            drawText(value,x+230-int(width),y,color,0);
            textSize(old);return;
        }
    } else if(*activeEntries==pickerEntries.data()) {
        font=0; // Compact native font: the full list fits 480- and 600-high views.
        for(unsigned i=0;i<pickerEntries.size();++i)if(y==int(pickerEntries[i].y+pickerMenu.textHeight)) {
            if(i==0) {
                font=2; // Font30 heading above the compact choices.
                swprintf_s(label,L"%s",saveFailed?L"Could not save color":(pickingWalls?L"Wall Color":L"Boundary Color"));
                color=saveFailed?1:4;
            } else if(i==pickerEntries.size()-1)swprintf_s(label,L"Back");
            else {
                const auto preset=static_cast<BoundaryColor>(i-1);
                const bool selected=preset==(pickingWalls?currentWallColor:currentColor)();
                swprintf_s(label,L"%s%s",boundaryPreset(preset).label,selected?L" (Selected)":L"");
            }
            break;
        }
    }
    if(!label[0]){originalText(cell,x,y,align,mode,extra);return;}
    auto old=textSize(font);
    DWORD width=0,file=0;textWidth(label,&width,&file);
    drawText(label,x-int(width)/2,y,color,0);
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
    // PD2 Escape invokes the last row of the active table. Keep that index in
    // sync on both submenu transitions, so Escape cannot call a color by mistake.
    if(at<const void*>(pd,0x22e7e1)!=pd+0x3d98e4 ||
       at<const void*>(client,0x65182)!=client+0x11c058 ||
       at<const void*>(client,0x65232)!=client+0x11c05c)return false;
    const unsigned char escapeCall[]={0x8b,0x84,0x30,0x14,0x01,0,0,0xff,0xd0};
    if(std::memcmp(pd+0x22e7ef,escapeCall,sizeof(escapeCall)))return false;
    return true;
}
inline bool install(unsigned char* pd,unsigned char* client,unsigned char* win,
                    SelectColor select,CurrentColor current,SelectColor selectWall,CurrentColor currentWall) {
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
        selectColor=select;currentColor=current;selectWallColor=selectWall;currentWallColor=currentWall;
        makeMenu(*reinterpret_cast<Menu*>(pd+0x39da90),reinterpret_cast<Entry*>(pd+0x3a3fb0));
        originalText=reinterpret_cast<CellText>(client+0xd372);
        drawText=reinterpret_cast<DrawText>(d);textSize=reinterpret_cast<TextSize>(s);textWidth=reinterpret_cast<TextWidth>(w);
        // Transfer already-loaded resources if initialization happened after
        // the native menu load. All three native load/select/free references
        // then use the owned table; no second owner frees these resources.
        bindActive(pd,client);
        for(auto& patch:patches){std::memcpy(patch.site,&patch.value,4);FlushInstructionCache(GetCurrentProcess(),patch.site,4);}
        installed=true;
    }
    while(ready){auto& patch=patches[--ready];DWORD ignored;VirtualProtect(patch.site,4,patch.protection,&ignored);}
    return installed;
}
}
