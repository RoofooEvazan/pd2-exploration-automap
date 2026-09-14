// Loaded-room collision reads. Only owned grid copies reach the worker.
#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>
#include <cctype>
#include <string>

// 1.13c layout. Game pointers remain on the capture thread; the worker owns copies.
namespace floor_reader {
struct Room {
    void* address=nullptr;void* next=nullptr;void* grid=nullptr;
    std::uint32_t level=0;
    int x=0,y=0,width=0,height=0;
};
struct AreaBounds {int x=0,y=0,width=0,height=0;};
template<class T> T field(const void* p,std::size_t offset) {
    return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(p)+offset);
}
inline void* firstRoom(const void* client) {
    __try {
        auto unit=field<void*>(client,0x11bbfc);if(!unit)return nullptr;
        auto act=field<void*>(unit,0x1c);return act?field<void*>(act,0x10):nullptr;
    } __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
inline bool currentAreaBounds(const void* client,AreaBounds* bounds) {
    __try {
        auto unit=field<void*>(client,0x11bbfc);if(!unit)return false;
        auto path=field<void*>(unit,0x2c);if(!path)return false;
        auto room=field<void*>(path,0x1c);if(!room)return false;
        auto room2=field<void*>(room,0x10);if(!room2)return false;
        auto level=field<void*>(room2,0x58);if(!level)return false;
        int x=field<int>(level,0x1c),y=field<int>(level,0x20);
        int w=field<int>(level,0x24),h=field<int>(level,0x28);
        if(x<0 || y<0 || w<1 || h<1 || w>512 || h>512 || x+w>13107 || y+h>13107)return false;
        // DRLG level rectangles use tiles; collision/player positions use subtiles.
        *bounds={x*5,y*5,w*5,h*5};
        auto grid=field<void*>(room,0x20);if(!grid)return false;
        int gx=field<int>(grid,0),gy=field<int>(grid,4),gw=field<int>(grid,8),gh=field<int>(grid,12);
        return gw>0 && gh>0 && gw<=512 && gh<=512 && gx>=bounds->x && gy>=bounds->y &&
            gx+gw<=bounds->x+bounds->width && gy+gh<=bounds->y+bounds->height;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool metadata(void* address,Room* room) {
    __try {
        room->address=address;room->next=field<void*>(address,0x7c);
        auto room2=field<void*>(address,0x10);if(!room2)return false;
        auto level=field<void*>(room2,0x58);if(!level)return false;
        room->level=field<std::uint32_t>(level,0x1d0);
        auto grid=field<void*>(address,0x20);if(!grid)return false;
        room->x=field<int>(grid,0);room->y=field<int>(grid,4);
        room->width=field<int>(grid,8);room->height=field<int>(grid,12);
        room->grid=field<void*>(grid,0x20);
        return room->grid && room->level>0 && room->level<10000 &&
            room->x>=0 && room->y>=0 && room->x<65536 && room->y<65536 &&
            room->width>0 && room->width<=512 && room->height>0 && room->height<=512 &&
            room->x+room->width<=65536 && room->y+room->height<=65536;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool copyGrid(const Room& room,std::uint16_t* destination) {
    __try {
        memcpy(destination,room.grid,std::size_t(room.width)*room.height*sizeof(std::uint16_t));return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
struct GroundTile {int x=0,y=0,type=0;char library[260]{};std::uint8_t collision[25]{};};
inline bool floorTiles(const Room& room,void** tiles,int* count) {
    __try {
        // D2Common 1.13c #10544: room+8 -> floor list at +8, count at +0xC.
        auto lists=field<void*>(room.address,8);if(!lists)return false;
        *tiles=field<void*>(lists,8);*count=field<int>(lists,12);
        return *count>=0 && *count<=16384 && (!*count || *tiles);
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool groundTile(const Room& room,const void* tiles,int index,GroundTile* result) {
    __try {
        auto tile=static_cast<const unsigned char*>(tiles)+std::size_t(index)*0x30;
        auto entry=field<void*>(tile,0x18);if(!entry || field<int>(tile,0x1c)!=0 || field<int>(entry,0x14)!=0)return false;
        int x=field<int>(tile,8),y=field<int>(tile,12);
        if(x<0 || y<0 || x>room.width/5 || y>room.height/5)return false;
        // D2CMP 1.13c #10035 returns the library name at +0x58;
        // #10011 returns the 25 floor collision bytes at +0x28.
        auto library=field<const char*>(entry,0x58);if(!library)return false;
        result->x=room.x+x*5;result->y=room.y+y*5;result->type=field<int>(entry,0x18);
        memcpy(result->collision,static_cast<const unsigned char*>(entry)+0x28,25);
        for(int i=0;i<260;++i){result->library[i]=library[i];if(!library[i])return true;}
        return false;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool grassyBankLibrary(const char* name) {
    std::string path(name);
    for(auto& c:path)c=c=='\\'?'/':char(std::tolower(static_cast<unsigned char>(c)));
    for(const char* file:{"act1/outdoors/pond.dt1","act1/outdoors/puddle.dt1","act1/outdoors/swamp.dt1"}) {
        const std::size_t length=std::char_traits<char>::length(file);
        if(path.size()>=length && path.compare(path.size()-length,length,file)==0 &&
           (path.size()==length || path[path.size()-length-1]=='/'))return true;
    }
    return false;
}
inline bool mapWaterLibrary(const char* name) {
    std::string path(name);
    for(auto& c:path)c=c=='\\'?'/':char(std::tolower(static_cast<unsigned char>(c)));
    for(const char* file:{"act1/outdoors/river.dt1","act2/outdoors/oasis.dt1","act3/river/rivbank.dt1",
        "pd2assets/psnwell/used/rivbank.dt1","pd2assets/psnwell/rivbank.dt1",
        "pd2assets/a5_river.dt1","pd2assets/a5_rotatedriver.dt1","pd2assets/dtprivate/oasis.dt1"}) {
        const auto length=std::char_traits<char>::length(file);
        if(path.size()>=length && path.compare(path.size()-length,length,file)==0 &&
           (path.size()==length || path[path.size()-length-1]=='/'))return true;
    }
    return false;
}
struct BankRead {unsigned tiles=0,banks=0,failures=0;};
inline std::vector<std::uint8_t> copyBanks(const Room& room,const std::vector<std::uint16_t>& grid,BankRead& stats) {
    std::vector<std::uint8_t> banks;
    if(grid.size()!=std::size_t(room.width)*room.height)return banks;
    void* tiles=nullptr;int count=0;
    if(!floorTiles(room,&tiles,&count)){++stats.failures;return banks;}
    for(int i=0;i<count;++i) {
        GroundTile tile{};
        if(!groundTile(room,tiles,i,&tile)){++stats.failures;continue;}
        ++stats.tiles;if(!grassyBankLibrary(tile.library) && !(room.level>132 && mapWaterLibrary(tile.library)))continue;
        ++stats.banks;if(banks.empty())banks.resize(grid.size());
        for(int y=tile.y;y<std::min(tile.y+5,room.y+room.height);++y)
            for(int x=tile.x;x<std::min(tile.x+5,room.x+room.width);++x) {
                const auto at=std::size_t(y-room.y)*room.width+x-room.x;
                // D2Common copies DT1 rows bottom to top. Require collision
                // from the floor itself: a prop on its dry half is not water.
                const auto material=tile.collision[(4-(y-tile.y))*5+x-tile.x];
                if((material&0x27)==1 && (grid[at]&0x27)==1)banks[at]=1;
            }
    }
    return banks;
}
template<class Wanted,class Visit> bool capture(const void* client,std::uint32_t level,Wanted wanted,Visit visit) {
    void* seen[256]{};int count=0;bool any=false;
    for(auto address=firstRoom(client);address && count<256;) {
        for(int i=0;i<count;++i)if(seen[i]==address)return any;
        seen[count++]=address;Room room;
        bool valid=metadata(address,&room);address=room.next;
        if(!valid || (level && room.level!=level) || !wanted(room))continue;
        std::vector<std::uint16_t> grid(std::size_t(room.width)*room.height);
        if(copyGrid(room,grid.data())){visit(room,grid);any=true;}
    }
    return any;
}
}
