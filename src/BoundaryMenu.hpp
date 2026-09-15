// In-memory extension of the profiled Automap Options menu.
// Native code owns navigation/resources; custom rows supply live text.
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
using SelectStyle=bool(*)(unsigned);
using CurrentStyle=unsigned(*)();
using CurrentThickness=double(*)();
inline constexpr unsigned mapsRow=8,campaignRow=9,backRow=10;
inline constexpr unsigned boundaryRow=1,wallRow=2,waterRow=3,thicknessRow=4,styleRow=5,stylingBackRow=6;
inline constexpr std::array<double,4> thicknessValues{0.5,1.0,1.5,2.0};
inline constexpr std::array<const char*,4> thicknessKeys{"0.5","1.0","1.5","2.0"};
enum class ColorTarget { Boundary, Wall, Water };
inline constexpr std::array<const wchar_t*,4> styleLabels{L"Original",L"Native",L"Hybrid",L"Styled"};
inline std::array<Entry,11> entries{};
inline std::array<Entry,7> stylingEntries{};
inline std::array<Entry,boundaryPresets.size()+2> pickerEntries{};
inline Menu menu{},stylingMenu{},pickerMenu{};
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
inline SelectColor selectWaterColor=nullptr;
inline CurrentColor currentWaterColor=nullptr;
inline SelectStyle selectStyle=nullptr;
inline CurrentStyle currentStyle=nullptr;
inline SelectStyle selectThickness=nullptr;
inline CurrentThickness currentThickness=nullptr;
inline bool thicknessSaveFailed=false;
inline bool styleSaveFailed=false;
inline bool installed=false,saveFailed=false;
inline ColorTarget picking=ColorTarget::Boundary;
inline bool editingMaps=true;
inline unsigned pickedRow() {return picking==ColorTarget::Water?waterRow:picking==ColorTarget::Wall?wallRow:boundaryRow;}
inline const wchar_t* pickedLabel() {return picking==ColorTarget::Water?L"Water Color":picking==ColorTarget::Wall?L"Wall Color":L"Boundary Color";}
inline CurrentColor pickedCurrent() {return picking==ColorTarget::Water?currentWaterColor:picking==ColorTarget::Wall?currentWallColor:currentColor;}
inline SelectColor pickedSelect() {return picking==ColorTarget::Water?selectWaterColor:picking==ColorTarget::Wall?selectWallColor:selectColor;}
inline const BoundaryPreset& pickedPreset(BoundaryColor color) {return picking==ColorTarget::Water?waterPreset(color):boundaryPreset(color);}
inline void activate(Menu& descriptor,Entry* rows,DWORD selected) {
    *activeEntries=rows;*activeMenu=&descriptor;
    *selection=selected;*escapeSelection=descriptor.count-1;
}
inline BOOL __fastcall back(Entry*,void*) {
    if(!activeEntries || !activeMenu || !selection || !escapeSelection)return FALSE;
    if(*activeEntries==pickerEntries.data() && *activeMenu==&pickerMenu)
        activate(stylingMenu,stylingEntries.data(),pickedRow());
    else if(*activeEntries==stylingEntries.data() && *activeMenu==&stylingMenu)
        activate(menu,entries.data(),editingMaps?mapsRow:campaignRow);
    else return FALSE;
    saveFailed=styleSaveFailed=thicknessSaveFailed=false;return TRUE;
}
inline BOOL openStyling(bool maps) {
    if(!activeEntries || !activeMenu || !selection || !escapeSelection ||
       *activeEntries!=entries.data() || *activeMenu!=&menu)return FALSE;
    editingMaps=maps;styleSaveFailed=thicknessSaveFailed=false;
    activate(stylingMenu,stylingEntries.data(),boundaryRow);return TRUE;
}
inline BOOL __fastcall openMaps(Entry*,void*) {return openStyling(true);}
inline BOOL __fastcall openCampaign(Entry*,void*) {return openStyling(false);}
inline BOOL __fastcall choose(Entry* row,void*) {
    if(!activeEntries || *activeEntries!=pickerEntries.data())return FALSE;
    for(unsigned i=0;i+2<pickerMenu.count;++i)if(row==&pickerEntries[i+1]) {
        const auto color=static_cast<BoundaryColor>(i);
        saveFailed=!pickedSelect()(color);
        if(!saveFailed)back(nullptr,nullptr);
        return TRUE;
    }
    return FALSE;
}
inline void configurePicker(ColorTarget target) {
    picking=target;
    pickerMenu.count=DWORD(pickerEntries.size());
    for(unsigned i=1;i<pickerEntries.size();++i)
        pickerEntries[i].press=i+1<pickerMenu.count?choose:i+1==pickerMenu.count?back:nullptr;
}
inline BOOL openPicker(ColorTarget target) {
    if(!activeEntries || !activeMenu || !selection || !escapeSelection ||
       *activeEntries!=stylingEntries.data() || *activeMenu!=&stylingMenu)return FALSE;
    configurePicker(target);saveFailed=false;
    auto color=static_cast<unsigned>(pickedCurrent()());
    if(color>=pickerMenu.count-2)color=static_cast<unsigned>(target==ColorTarget::Boundary?BoundaryColor::Cyan:BoundaryColor::White);
    activate(pickerMenu,pickerEntries.data(),color+1);return TRUE;
}
inline BOOL __fastcall openBoundary(Entry*,void*) {return openPicker(ColorTarget::Boundary);}
inline BOOL __fastcall openWalls(Entry*,void*) {return openPicker(ColorTarget::Wall);}
inline BOOL __fastcall openWater(Entry*,void*) {return openPicker(ColorTarget::Water);}
inline BOOL __fastcall cycleStyle(Entry* row,void*) {
    if(!activeEntries || !activeMenu || *activeEntries!=stylingEntries.data() || *activeMenu!=&stylingMenu ||
       row!=&stylingEntries[styleRow] || !selectStyle || !currentStyle)return FALSE;
    styleSaveFailed=!selectStyle((currentStyle()+1)%styleLabels.size());return TRUE;
}
inline BOOL __fastcall cycleThickness(Entry* row,void*) {
    if(!activeEntries || !activeMenu || *activeEntries!=stylingEntries.data() || *activeMenu!=&stylingMenu ||
       row!=&stylingEntries[thicknessRow] || !selectThickness || !currentThickness)return FALSE;
    const auto next=std::upper_bound(thicknessValues.begin(),thicknessValues.end(),currentThickness());
    const unsigned choice=next==thicknessValues.end()?0:static_cast<unsigned>(next-thicknessValues.begin());
    thicknessSaveFailed=!selectThickness(choice);return TRUE;
}
inline void bindActive(unsigned char* pd,unsigned char* client) {
    activeEntries=reinterpret_cast<Entry**>(client+0x11c060);
    activeMenu=reinterpret_cast<Menu**>(client+0x11c05c);
    selection=reinterpret_cast<DWORD*>(client+0x11c058);
    escapeSelection=reinterpret_cast<DWORD*>(pd+0x3d98e4);
    // The copied main table is the sole owner used by native load/free.
    // The picker has no resources and is never passed to either operation.
    if(*activeMenu==reinterpret_cast<Menu*>(pd+0x39da90) &&
       *activeEntries==reinterpret_cast<Entry*>(pd+0x3a3fb0)) {
        const auto selected=*selection==8?backRow:*selection;
        activate(menu,entries.data(),selected<menu.count?selected:backRow);
    }
}
inline void makeMenu(const Menu& source,const Entry* sourceEntries) {
    menu=source;menu.count=DWORD(entries.size());menu.spacing=36;menu.textHeight=32;menu.barHeight=32;
    std::copy_n(sourceEntries,8,entries.begin());
    entries[mapsRow]=Entry{};entries[mapsRow].press=openMaps;
    entries[campaignRow]=Entry{};entries[campaignRow].press=openCampaign;
    entries[backRow]=sourceEntries[8];
    stylingMenu=source;stylingMenu.count=DWORD(stylingEntries.size());
    stylingEntries={};stylingEntries[0].type=0xffffffff;
    stylingEntries[boundaryRow].press=openBoundary;stylingEntries[wallRow].press=openWalls;
    stylingEntries[waterRow].press=openWater;
    stylingEntries[thicknessRow].press=cycleThickness;
    stylingEntries[styleRow].press=cycleStyle;stylingEntries[stylingBackRow].press=back;
    pickerMenu=source;pickerMenu.count=DWORD(pickerEntries.size());
    pickerMenu.spacing=24;pickerMenu.textHeight=22;pickerMenu.barHeight=23;
    pickerEntries={};pickerEntries[0].type=0xffffffff;
    configurePicker(ColorTarget::Boundary);
}
inline void __fastcall drawRow(void* cell,int x,int y,int align,int mode,int extra) {
    if(cell || !activeEntries) {
        originalText(cell,x,y,align,mode,extra);return;
    }
    wchar_t label[96]{};
    DWORD font=2,color=4; // Native Font30 matches the existing option artwork.
    if(*activeEntries==entries.data()) {
        if(y==int(entries[mapsRow].y+menu.textHeight))swprintf_s(label,L"Maps Styling");
        else if(y==int(entries[campaignRow].y+menu.textHeight))swprintf_s(label,L"Campaign Styling");
    } else if(*activeEntries==stylingEntries.data()) {
        const wchar_t* value=nullptr;
        wchar_t thicknessText[32]{};
        if(y==int(stylingEntries[0].y+stylingMenu.textHeight))
            swprintf_s(label,L"%s",editingMaps?L"Maps Styling":L"Campaign Styling");
        else if(y==int(stylingEntries[stylingBackRow].y+stylingMenu.textHeight))swprintf_s(label,L"Back");
        else if(y==int(stylingEntries[styleRow].y+stylingMenu.textHeight) && currentStyle) {
            swprintf_s(label,L"Stylization");
            const unsigned selected=currentStyle();
            value=styleSaveFailed?L"Save failed":styleLabels[selected<styleLabels.size()?selected:1];
            if(styleSaveFailed)color=1;
        } else if(y==int(stylingEntries[boundaryRow].y+stylingMenu.textHeight))
            {swprintf_s(label,L"Boundary Color");value=boundaryPreset(currentColor()).label;}
        else if(y==int(stylingEntries[wallRow].y+stylingMenu.textHeight))
            {swprintf_s(label,L"Wall Color");value=boundaryPreset(currentWallColor()).label;}
        else if(y==int(stylingEntries[waterRow].y+stylingMenu.textHeight))
            {swprintf_s(label,L"Water Color");value=waterPreset(currentWaterColor()).label;}
        else if(y==int(stylingEntries[thicknessRow].y+stylingMenu.textHeight) && currentThickness) {
            swprintf_s(label,L"Boundary Thickness");swprintf_s(thicknessText,L"%g",currentThickness());
            if(!wcschr(thicknessText,L'.'))wcscat_s(thicknessText,L".0");
            value=thicknessSaveFailed?L"Save failed":thicknessText;
            if(thicknessSaveFailed)color=1;
        }
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
        for(unsigned i=0;i<pickerMenu.count;++i)if(y==int(pickerEntries[i].y+pickerMenu.textHeight)) {
            if(i==0) {
                font=2; // Font30 heading above the compact choices.
                swprintf_s(label,L"%s",saveFailed?L"Could not save color":pickedLabel());
                color=saveFailed?1:4;
            } else if(i==pickerMenu.count-1)swprintf_s(label,L"Back");
            else {
                const auto preset=static_cast<BoundaryColor>(i-1);
                const bool selected=preset==pickedCurrent()();
                swprintf_s(label,L"%s%s",pickedPreset(preset).label,selected?L" (Selected)":L"");
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
                    SelectColor select,CurrentColor current,SelectColor selectWall,CurrentColor currentWall,
                    SelectColor selectWater,CurrentColor currentWater,
                    SelectStyle chooseStyle,CurrentStyle selectedStyle,
                    SelectStyle chooseThickness,CurrentThickness selectedThickness) {
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
        selectWaterColor=selectWater;currentWaterColor=currentWater;
        selectStyle=chooseStyle;currentStyle=selectedStyle;
        selectThickness=chooseThickness;currentThickness=selectedThickness;
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
